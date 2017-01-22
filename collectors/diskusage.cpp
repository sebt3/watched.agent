#include "collectors.h"
#include <chrono>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#include <stdio.h>
#include <mntent.h>
#include <sys/statvfs.h>

using namespace watcheD;
namespace collectors {

std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}


class diskUsageRess : public Ressource {
public:
	diskUsageRess(uint p_size) : Ressource(p_size, "disk_usage") {
		addProperty("size",  "Total size (Mb)", "integer");
		addProperty("free",  "Free storage (Mb)", "integer");
		addProperty("pctfree",  "Free space %", "number");
		addProperty("ipctfree",  "Free inode %", "number");
	}
	void setStats(struct statvfs *p_stats) {
		nextValue();
		setProperty("size", p_stats->f_bsize*p_stats->f_blocks/(1024*1024));
		setProperty("free", p_stats->f_bsize*p_stats->f_bavail/(1024*1024));
		setProperty("pctfree", (double)(p_stats->f_bavail*100)/p_stats->f_blocks);
		setProperty("ipctfree", (double)(p_stats->f_favail*100)/p_stats->f_files);
	}
};

class diskUsageCollector : public Collector {
public:
	diskUsageCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("diskusage", p_srv, p_cfg, 25, 60) {
		addGetMetricRoute();
	}

	void collect() {
		FILE *fh;
		struct mntent *mnt_info;
		struct statvfs stats;
		
		std::string id;
		std::shared_ptr<diskUsageRess> res;

		if ( ( fh = setmntent( "/etc/mtab", "r" ) ) == NULL ) {
			std::cerr << "Cannot open \'/etc/mtab\'!\n";
			return;
		}

		while ( ( mnt_info = getmntent( fh ) ) != NULL ) {
			if ( (mnt_info->mnt_dir[0] != '/') || !( (mnt_info->mnt_fsname[0] == '/') || !strcmp( mnt_info->mnt_type, "tmpfs" ) || !strcmp( mnt_info->mnt_type, "udev" ) ) )
				continue;
			if ( statvfs( mnt_info->mnt_dir, &stats ) < 0 )
				continue;

			id = url_encode(mnt_info->mnt_dir);
			if (ressources.find(id) == ressources.end()) {
				res = std::make_shared<diskUsageRess>((*cfg)["history"].asUInt());
				ressources[id] = res;
				desc[id] = mnt_info->mnt_dir;
				desc[id]+= " disk usage";
			}else
				res = reinterpret_cast<std::shared_ptr<diskUsageRess>&>(ressources[id]);
			res->setStats(&stats);
		}
		endmntent( fh );

	}
};


MAKE_PLUGIN_COLLECTOR(diskUsageCollector, diskusage)

}
