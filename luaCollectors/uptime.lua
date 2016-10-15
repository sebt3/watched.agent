cfg = {}
cfg["name"] = "uptime"
cfg["poolfreq"] = 100
cfg["history"]  = 300


function declare ()
	this.addRessource("uptime", "Load average", "uptime")
	this.addProperty( "uptime", "uptime",  "System uptime", "number")
end


function collect ()
	local f = assert(io.open("/proc/uptime", "r"))
	local t = f:read("*all")
	f:close()
	this.nextValue("uptime")
	this.setDProperty("uptime", "uptime", t:match("(%d+.%d+)"))
end
