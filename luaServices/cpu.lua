types = {"collector", "enhancer"}

cfg = {}
cfg["name"] = "cpu"
cfg["poolfreq"] = 10
cfg["history"]  = 150


function declare ()
	this.addRessource("shed", "Service scheduler information", "service_cpu_shed")
	this.addProperty( "shed", "exec_runtime",  "exec_runtime", "number")
	this.addProperty( "shed", "process",  "Number of process", "number")
	this.addProperty( "shed", "thread",  "total number of threads", "number")
	this.addRessource("stat", "Service CPU usage information", "service_cpu_stat")
	this.addProperty( "stat", "user",    "user %",   "number")
	this.addProperty( "stat", "system",  "system %", "number")
	this.addProperty( "stat", "pct",  "total % used", "number")
	this.addProperty( "stat", "cuser",   "user % cumulated",   "number")
	this.addProperty( "stat", "csystem", "system % cumulated", "number")
end

values = {}
users  = {}
sys    = {}
cusers = {}
csys   = {}
prev_total_time = 0

function collect ()
	local f
	local t
	local i          = 1
	local sum        = 0.0
	local process    = 0
	local thread     = 0
	local total_time = 0
	local cpu_count  = 0
	local res_users  = 0
	local res_sys    = 0
	local res_cusers = 0
	local res_csys   = 0
	-- get total_time 1st
	f = assert(io.open("/proc/stat", "r"))
	for line in f:lines() do
		if (line:match("(%a+%d*)") == "cpu") then
			for part in string.gmatch(line, "%d+") do
				total_time  = total_time + tonumber(part)
			end
		elseif (line:match("(%a+)") == "cpu") then
			cpu_count=cpu_count+1
		end
	end
	f:close()
	if (cpu_count==0) then
		cpu_count=1
	end
	this.getPIDList()
	while (type(pids[i]) == "number")
	do
		f = io.open("/proc/".. pids[i] .."/sched", "r")
		if f ~= nill then
			for line in f:lines() do
				if (line:match("(%a+.%a+_%a+_%a+)") == "se.sum_exec_runtime") then
					val = line:match("(%d+.%d+)")
					if(type(values[pids[i] .. ""]) == "number") then
						sum = sum + tonumber(val) - values[pids[i] .. ""]
					end
					values[pids[i] .. ""] = tonumber(val)
				elseif (line:match("threads") == "threads") then
					thread = thread + tonumber(line:match(".*: (%d+)"))
				end
			end
			f:close()
		end
		f = io.open("/proc/".. pids[i] .."/stat", "r")
		if f~=nil then
			t = f:read("*all")
			f:close()
		else
			t = ""
		end
		local n = 3
		for part in string.gmatch(t, "%d+") do
			if n == 14 then
				if(type(users[pids[i] .. ""]) == "number") then
					res_users = res_users+tonumber(part)-users[pids[i] .. ""]
				end
				users[pids[i] .. ""] = tonumber(part)
			elseif n == 15 then
				if(type(sys[pids[i] .. ""]) == "number") then
					res_sys = res_sys+tonumber(part)-sys[pids[i] .. ""]
				end
				sys[pids[i] .. ""] = tonumber(part)
			elseif n == 16 then
				if(type(cusers[pids[i] .. ""]) == "number") then
					res_cusers = res_cusers+tonumber(part)-cusers[pids[i] .. ""]
				end
				cusers[pids[i] .. ""] = tonumber(part)
			elseif n == 17 then
				if(type(csys[pids[i] .. ""]) == "number") then
					res_csys = res_csys+tonumber(part)-csys[pids[i] .. ""]
				end
				csys[pids[i] .. ""] = tonumber(part)
			end
			n=n+1
		end
		i=i+1
		process=process+1
	end
	if prev_total_time ~= 0 then
		this.nextValue("shed")
		this.setDProperty("shed", "exec_runtime", sum)
		this.setDProperty("shed", "process", process)
		this.setDProperty("shed", "thread", thread)
		this.nextValue("stat")
		this.setDProperty("stat", "user",    res_users*100*cpu_count/(total_time-prev_total_time))
		this.setDProperty("stat", "system",  res_sys*100*cpu_count/(total_time-prev_total_time))
		this.setDProperty("stat", "pct",  (res_sys+res_users)*100*cpu_count/(total_time-prev_total_time))
		this.setDProperty("stat", "cuser",   res_cusers*100*cpu_count/(total_time-prev_total_time))
		this.setDProperty("stat", "csystem", res_csys*100*cpu_count/(total_time-prev_total_time))
	end
	prev_total_time = total_time
end

function enhance(serv)
	-- adding this collector to every service
	serv.addCollector("cpu")
end
