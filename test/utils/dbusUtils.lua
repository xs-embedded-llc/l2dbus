local ldbus = require("ldbus")
local cjson = require("cjson")

local M = {}

for k,v in pairs(ldbus) do
    M[k] = v
end

function M.openBus(busName, privConn)

	local self = {}
	self.priv = {
			busName			= busName or ldbus.SESSION_BUS,
			bus				= nil,
			defaultAdaptor	= nil, -- assigned default
	        defaultProxy	= nil  -- assigned default	
	}
	
	self.newAdaptor = function(objPath)
		local adaptor, errInfo = self.priv.bus.newAdaptor(objPath)
		return adaptor, errInfo.errMsg or ""
	end
	
	self.newProxy = function(busName, objPath)
		local proxy, errInfo = self.priv.bus.newProxy(busName, objPath)
		
		return proxy, errInfo.errMsg or ""
	end

	self.requestName = function(busName)
		local result, errInfo = self.priv.bus.requestName(busName)
		
		return result, errInfo.errMsg or ""
	end
	
	self.registerGenReqHdlr = function(handlerList, adaptor)
		adaptor = adaptor or self.priv.defaultAdaptor
		
		local function onRequest(ctx, method, payload, sender, serial)
			local handler = handlerList[method]
			if "function" == type(handler) then
				local ctxContainer = {
					priv = self.priv,
					context = ctx,
					agent = adaptor,
					method = method,
					sender = sender,
					
					reply = function(response)
						if "table" == type(response) then
							local sResp, sErr = cjson.encode(response)
							if sResp == nil then
								return false, "Error: unable to encode table to JSON: " .. sErr
							end
							response = sResp
						end
						assert( "string" == type(response) )
						local result, errInfo = adaptor.reply(ctx, response)
						return result, errInfo.errMsg or ""
					end,
					
					replyError = function(errMsg, errName)
						print("errMsg: " .. tostring(errMsg))
						if "table" == type(errMsg) then
							local sResp, sErr = cjson.encode(errMsg)
							if sResp == nil then
								return false, "Error: unable to encode table to JSON: " .. sErr
							end
							errMsg = sResp
						end
						assert( "string" == type(errMsg) )
						local result, errInfo = adaptor.dbusError(ctx, errName, errMsg)
						return result, errInfo.errMsg or ""
					end
				}
				
				local result, errMsg = handler(payload, ctxContainer)
				if result ~= nil then
					return ctxContainer.reply(result)	
				elseif errMsg ~= nil then
					return ctxContainer.replyError(errMsg, nil)
				end  
			else
				adaptor.dbusError(ctx, nil,
						string.format("unknown method <%s>", method));
			end
		end
		
		return adaptor.registerMethod(nil, onRequest)
	end
	
	self.setDefaultAdaptor = function(adaptor)
		self.priv.defaultAdaptor = adaptor
	end
	
	self.setDefaultProxy = function(proxy)
		self.priv.defaultProxy = proxy
	end
	
	self.close = function()
		local result, errInfo = self.priv.bus.close()
		return result, errInfo.errMsg or ""
	end
	
	self.request = function(method, data, proxy, ignoreReply)
		proxy = proxy or self.priv.defaultProxy
		
		if "table" == type(data) then
			local sData, sErr = cjson.encode(data)
			if sData == nil then
				return false, "Error: unable to encode table to JSON: " .. sErr
			end
			data = sData
		end
		
		assert("string" == type(data))
		
		local result, errInfo = proxy.request(method, data, ignoreReply)
		local status
		if errInfo.errCode == ldbus.ERR_OK then
			status = nil
		else
			status = errInfo.errMsg or ""
		end
		return result, status
	end
	

	self.getMaxReceivedSize = function()
		return self.priv.bus.getMaxReceivedSize()
	end
	

	self.setMaxReceivedSize = function(size)
		self.priv.bus.setMaxReceivedSize(size)
	end
	

	self.getOutgoingSize = function()
		return self.priv.bus.getOutgoingSize()
	end
	
	
	-- openBus
	local errInfo
	self.priv.bus, errInfo = ldbus.openBus(busName and busName or self.priv.busName,
									  privConn)
	local result = self.priv.bus and self or nil
	return result, errInfo.errMsg or ""	
		
end



return M
