types = {"logParser", "enhancer"}

cfg = {}
cfg["poolfreq"] = 60
cfg["history"]  = 25


function getLevel (text)
	if not (string.find(text, "ERROR") == nil) then
		return error
	end
	if not (string.find(text, "WARN") == nil) then
		return warning
	end
	return ok
end

function getDate (text)
	return text:match("%[(.-)%|")
end

function enhance(serv)
	if serv.isSelf() then
		serv.addCollector("self")
		serv.addLogMonitor("self", "watched.agent")
	end
end
