types = {}
table.insert(types, "collector")
table.insert(types, "enhancer")

cfg = {}
cfg["name"] = "cpu"
cfg["poolfreq"] = 5
cfg["history"]  = 300


function declare ()
	this.addRessource("cpu", "Service CPU usage", "serviceCPU")
	this.addProperty( "cpu", "used",  "Used CPU", "number")
end

values = {}

function collect ()
	local f
	local i=1
	this.nextValue("cpu")
	sum = 0.0
	this.getPIDList()
	while (type(pids[i]) == "number")
	do
		f = assert(io.open("/proc/".. pids[i] .."/sched", "r"))
		for line in f:lines() do
			if (line:match("(%a+.%a+_%a+_%a+)") == "se.sum_exec_runtime") then
				val = line:match("(%d+.%d+)")
				if(type(values[pids[i] .. ""]) == "number") then
					sum = sum + tonumber(val) - values[pids[i] .. ""]
				end
				values[pids[i] .. ""] = tonumber(val)
			end
		end
		i=i+1
	end
	this.setDProperty("cpu", "used", sum)
end

function enhance(serv)
	-- adding this collector to every service
	io.write(string.format("Adding cpu collector to a service\n"))
	serv.addCollector("cpu")
end
