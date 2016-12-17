types = {}
table.insert(types, "collector")
table.insert(types, "enhancer")

cfg = {}
cfg["name"] = "io"
cfg["poolfreq"] = 10
cfg["history"]  = 150


function declare ()
	this.addRessource("io", "Service IO usage", "service_io")
	this.addProperty( "io", "read",  "Read bytes", "number")
	this.addProperty( "io", "write",  "Write bytes", "number")
end

reads  = {}
writes = {}

function collect ()
	local f
	local i          = 1
	local res_reads  = 0
	local res_writes = 0
	local found      = false
	this.getPIDList()
	while (type(pids[i]) == "number")
	do
		f = assert(io.open("/proc/".. pids[i] .."/io", "r"))
		for line in f:lines() do
			if (line:match("(%a+_%a+)") == "read_bytes") then
				val = line:match("(%d+)")
				if(type(reads[pids[i] .. ""]) == "number") then
					res_reads = res_reads + tonumber(val) - reads[pids[i] .. ""]
					found = true
				end
				reads[pids[i] .. ""] = tonumber(val)
			elseif (line:match("(%a+_%a+)") == "write_bytes") then
				val = line:match("(%d+)")
				if(type(writes[pids[i] .. ""]) == "number") then
					res_writes = res_writes + tonumber(val) - writes[pids[i] .. ""]
					found = true
				end
				writes[pids[i] .. ""] = tonumber(val)
			end
		end
		f:close()
		i=i+1
	end
	if found then
		this.nextValue("io")
		this.setDProperty("io", "read", res_reads)
		this.setDProperty("io", "write", res_writes)
	end
	prev_total_time = total_time
end

function enhance(serv)
	-- adding this collector to every service
	serv.addCollector("io")
end