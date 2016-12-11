-- Define what kind of service plugin this is
types = {}
table.insert(types, "collector")

-- Collector stuff
cfg = {}
cfg["name"] = "cpu"
cfg["poolfreq"] = 5
cfg["history"]  = 300


function declare ()
	this.addRessource("cpu", "Service CPU usage", "serviceCPU")
	this.addProperty( "cpu", "used",  "Used CPU", "number")
end

proc  = {}

function collect ()
	this.nextValue("cpu")
	sum = 1
	this.getPIDList()
	-- TODO: loop over the pids and get cpu used by each of them
	this.setDProperty("cpu", "used", sum)
end
