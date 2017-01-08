types = {}
table.insert(types, "enhancer")

function enhance(serv)
	lookup = lookup or { --			type		subType
		["mysqld"]		= { "database",		"mysql"		},
		["avahi-daemon"]	= { "desktop",		"apple"		},
		["chromium"]		= { "desktop",		"browser"	},
		["steam"]		= { "desktop",		"game"		},
		["kdeconnectd"]		= { "desktop",		"kde"		},
		["watched.back"]	= { "monitoring",	"backend"	},
		["watched.agent"]	= { "monitoring",	"agent"		},
		["accounts-daemon"]	= { "system",		"auth"		},
		["sddm"]		= { "system",		"auth"		},
		["systemd-logind"]	= { "system",		"auth"		},
		["dbus-daemon"]		= { "system",		"core"		},
		["systemd"]		= { "system",		"core"		},
		["acpid"]		= { "system",		"devices"	},
		["bluetoothd"]		= { "system",		"devices"	},
		["systemd-udevd"]	= { "system",		"devices"	},
		["upowerd"]		= { "system",		"devices"	},
		["rsyslogd"]		= { "system",		"log"		},
		["systemd-journald"]	= { "system",		"log"		},
		["exim4"]		= { "system",		"mail"		},
		["dhclient"]		= { "system",		"network"	},
		["ModemManager"]	= { "system",		"network"	},
		["NetworkManager"]	= { "system",		"network"	},
		["wpa_supplicant"]	= { "system",		"network"	},
		["packagekitd"]		= { "system",		"packages"	},
		["cups-browsed"]	= { "system",		"print"		},
		["cupsd"]		= { "system",		"print"		},
		["rtkit-daemon"]	= { "system",		"realtime"	},
		["rpcbind"]		= { "system",		"rpc"		},
		["rpc.statd"]		= { "system",		"rpc"		},
		["atd"]			= { "system",		"scheduling"	},
		["cron"]		= { "system",		"scheduling"	},
		["polkitd"]		= { "system",		"security"	},
		["sshd"]		= { "system",		"ssh"		},
		["udisksd"]		= { "system",		"storage"	},
		["lvmetad"]		= { "system",		"storage"	},
		["agetty"]		= { "system",		"terminal"	},
		["systemd-timesyncd"]	= { "system",		"time"		},
		["libvirtd"]		= { "system",		"virtualisation" },
		["apache"]		= { "web",		"apache"	},
		["apache2"]		= { "web",		"apache"	},
		["php7.0"]		= { "web",		"php"		}
	}
	local name = serv.getName()
	local arr = lookup[name] or { "", "" }
	local type = arr[1] or ""
	local sub  = arr[2] or ""
	if not (type == "") then
		serv.setType(type)
	--else
	--	print(name)
	end
	if not (sub == "") then
		serv.setSubType(sub)
	end
	if name == "rpc.statd" or name == "avahi-daemon" then
		serv.setUniqKey("dynamic")
		serv.updateBasePaths()
		-- as its base port is not fixed, its id would change from boot to boot
		-- which is not desirable. So hard coding one here
	end
end
