types = {}
table.insert(types, "enhancer")

function enhance(serv)
	lookup = lookup or { --			type		subType
		["kdeconnectd"]		= { "desktop",		"kde"		},
		["chromium"]		= { "desktop",		"browser"	},
		["avahi-daemon"]	= { "desktop",		"apple"		},
		["rpcbind"]		= { "system",		"rpc"		},
		["rpc.statd"]		= { "system",		"rpc"		},
		["exim4"]		= { "system",		"mail"		},
		["cupsd"]		= { "system",		"print"		},
		["sshd"]		= { "system",		"ssh"		},
		["dhclient"]		= { "system",		"dhcp"		},
		["cups-browsed"]	= { "system",		"print"		},
		["apache"]		= { "web",		"apache"	},
		["apache2"]		= { "web",		"apache"	},
		["php7.0"]		= { "web",		"php"		},
		["mysqld"]		= { "database",		"mysql"		},
		["watched.back"]	= { "monitoring",	"backend"	},
		["watched.agent"]	= { "monitoring",	"frontend"	}
	}
	local name = serv.getName()
	local type = lookup[name][1] or ""
	local sub  = lookup[name][2] or ""
	if not (type == "") then
		serv.setType(type)
	end
	if not (sub == "") then
		serv.setSubType(sub)
	end
	if name == "rpc.statd" then
		serv.setUniqKey("dynamic")
		-- as its base port is not fixed, its id would change from boot to boot
		-- which is not desirable. So hard coding one here
	end
end
