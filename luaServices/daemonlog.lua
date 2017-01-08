types = {"logParser", "enhancer"}

cfg = {}
cfg["poolfreq"] = 60
cfg["history"]  = 25


function getLevel (text)
	if not (string.find(text, "error") == nil) then
		return error
	end
	return ok
end

function getDate (text)
	return text:match("%[(.-)%]")
end

function enhance(serv)
	local name = serv.getName()
	if name == "rsyslogd" then
		serv.addLogMonitor("daemonlog", "daemon")
	end
end
