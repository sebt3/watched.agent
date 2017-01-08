types = {}
table.insert(types, "collector")
table.insert(types, "enhancer")

cfg = {}
cfg["name"] = "mem"
cfg["poolfreq"] = 60
cfg["history"]  = 25


function declare ()
	this.addRessource("mem", "Service memory usage", "serviceMemory")
	this.addProperty( "mem", "total",   "Total Memory usage (kB)", "number")
	this.addProperty( "mem", "swap",    "Swapped (kB)", "number")
	this.addProperty( "mem", "shared",  "Shared Libraries (kB)", "number")
end

function collect ()
	local f
	local i=1
	this.nextValue("mem")
	local total  = 0
	local shared = 0
	local swap   = 0
	local key    = ""
	this.getPIDList()
	while (type(pids[i]) == "number")
	do
		f = io.open("/proc/".. pids[i] .."/smaps", "r")
		if f~=nil then
			for line in f:lines() do
				key = line:match("(%a+)")
				if (key == "Rss") then
					val = line:match("(%d+)")
					total = total + tonumber(val)
				elseif (key == "Swap") then
					val = line:match("(%d+)")
					swap = swap + tonumber(val)
				elseif (key == "Shared_Clean") then
					val = line:match("(%d+)")
					shared = shared + tonumber(val)
				end
			end
			f:close()
		end
		i=i+1
	end
	this.setDProperty("mem", "swap",   swap)
	this.setDProperty("mem", "total",  total)
	this.setDProperty("mem", "shared", shared)
end

function enhance(serv)
	-- adding this collector to every service
	--io.write(string.format("Adding cpu collector to a service\n"))
	serv.addCollector("mem")
end
