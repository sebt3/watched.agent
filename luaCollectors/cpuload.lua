cfg = {}
cfg["name"] = "cpuload"
cfg["poolfreq"] = 10
cfg["history"]  = 150


function declare ()
	this.addRessource("avg", "Load average", "loadaverage")
	this.addProperty( "avg", "avg1",  "Load average (1mn)", "number")
	this.addProperty( "avg", "avg5",  "Load average (5mn)", "number")
	this.addProperty( "avg", "avg15", "Load average (15mn)", "number")
end


function collect ()
	local f = assert(io.open("/proc/loadavg", "r"))
	local t = f:read("*all")
	f:close()
	a,b,c = t:match("(%d+.%d+) (%d+.%d+) (%d+.%d+)")
	this.nextValue("avg")
	this.setDProperty("avg", "avg1", a)
	this.setDProperty("avg", "avg5", b)
	this.setDProperty("avg", "avg15", c)
end
