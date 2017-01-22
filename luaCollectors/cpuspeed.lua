cfg = {}
cfg["name"] = "cpuspeed"
cfg["poolfreq"] = 5
cfg["history"]  = 300


function declare ()
	local f = assert(io.open("/proc/cpuinfo", "r"))
	local id
	local line = ""
	for line in f:lines() do
		if (line:match("(%a+)") == "processor") then
			id = line:match("(%d+)")
			this.addRessource(id, "CPU " .. id .. " speed (Mhz)", "cpuspeed")
			this.addProperty( id, "MHz", "CPU speed (Mhz)", "number")
		end
	end
	f:close()
end


function collect ()
	local f = assert(io.open("/proc/cpuinfo", "r"))
	local id
	local line = ""
	for line in f:lines() do
		if (line:match("(%a+)") == "processor") then
			id = line:match("(%d+)")
		elseif (line:match("(%a+ %a+)") == "cpu MHz")  then
			this.nextValue(id)
			this.setDProperty(id, "MHz", line:match("(%d+.%d+)"))
		end
	end
	f:close()
end
