function log(text) {
  if (console && console.log) console.log(text);
  return text;
}

if (!Date.now) {
    Date.now = function() { return new Date().getTime(); }
}

d3.dispatch.prototype.register = function() {
	for (var i = 0, n = arguments.length, _ = {}, t; i < n; ++i) {
		if (!(t = arguments[i] + "") || (t in _)) throw new Error("illegal type: " + t);
		this._[t] = [];
	}	
}

/////////////////////////////////////////////////////////////////////////////////////////////
// watched Common Component
function wdDateFormat(date) {
	var	locale = d3.timeFormatLocale({
			"dateTime": "%A, le %e %B %Y, %X",
			"date": "%Y-%m-%d",
			"time": "%H:%M",
			"periods": ["AM", "PM"],
			"days": ["dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"],
			"shortDays": ["dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."],
			"months": ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"],
			"shortMonths": ["janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."]
		}),formatMillisecond	= locale.format(".%L"),
		formatSecond		= locale.format(":%S"),
		formatMinute		= locale.format("%X"),
		formatHour		= locale.format("%X"),
		formatDay		= locale.format("%x"),
		formatWeek		= locale.format("%x"),
		formatMonth		= locale.format("%x"),
		formatYear		= locale.format("%Y");
	return (d3.timeSecond(date) < date ? formatMillisecond
		: d3.timeMinute(date) < date ? formatSecond
		: d3.timeHour(date) < date ? formatMinute
		: d3.timeDay(date) < date ? formatHour
		: d3.timeMonth(date) < date ? (d3.timeWeek(date) < date ? formatDay : formatWeek)
		: d3.timeYear(date) < date ? formatMonth
		: formatYear)(date);
}

function wdBaseComponant() {
	var	data	= [],
		called	= false,
		root;
	function chart(s) { called=true; s.each(chart.init); return chart; }

	chart.callbacks = {};
	chart.dispatch	= d3.dispatch("init", "renderUpdate", "dataUpdate");
	chart.inited	= function() {return called; }
	chart.init	= function() { 
		root = d3.select(this);
		chart.dispatch.call("init");
		chart.dispatch.call("renderUpdate");
	}
	chart.root	= function(_) {
		if (arguments.length) {
			root = _;
			return chart;
		} else if (chart.inited())
			return root; 
		else
			return false;
	}
	chart.data	= function(_) { 
		if (!arguments.length) return data;
		if (typeof _!="object" || typeof _[0]=="undefined") return chart;
		data = _;
		chart.dispatch.call("dataUpdate");
		if (chart.inited())
			chart.dispatch.call("renderUpdate");
		return chart;
	}
	return chart;
}

function wdFilteredComponant(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdBaseComponant(),
		keys	= [],
		filter	= function(e){return e!="timestamp"&&!e.match("min_")&&!e.match("max_")/**/;};

	chart.filter	= function(_) { if (!arguments.length) return filter; filter = _; return chart; }
	chart.keys	= function(_) { 
		if (!arguments.length) return keys;
		keys = _;
	}
	chart.dispatch.on("dataUpdate.wdFilteredComponant", function() { 
		chart.keys(Object.keys(chart.data()[0]).filter(filter))
	});
	return chart;
}

function wdColoredComponant(pClass, pColor) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdBaseComponant(),
		color	= (typeof pColor!="undefined"&&pColor!=null)?pColor:d3.scaleOrdinal(d3.schemeCategory10);
	chart.dispatch.register("colorUpdate");
	chart.color	= function(_) { 
		if (!arguments.length) return color; color = _;
		chart.dispatch.call("colorUpdate");
		return chart;
	}
	return chart;
}

function wdPeriodComponant(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdBaseComponant(),
		baseUrl	= "",
		download	= function(url) {
		$.getJSON(url, function(results) {
			chart.data(results); 
		});
		return chart;
	};
	chart.baseUrl	= function(_) {
		if (!arguments.length) return baseUrl; baseUrl = _;
		download(baseUrl);
		return chart;
	};
	chart.setPeriod	= function(p) {
		var url;
		switch(p) {
			case "month": url=baseUrl+"/"+(Math.floor(Date.now())-(3600000*24*31));break;
			case "week": url=baseUrl+"/"+(Math.floor(Date.now())-(3600000*24*7));break;
			case "yesterday": url=baseUrl+"/"+((Math.floor(Date.now()/(3600000*24))-1)*(3600000*24))+"/"+(Math.floor(Date.now()/(3600000*24))*(3600000*24));break;
			case "today": url=baseUrl+"/"+(Math.floor(Date.now()/(3600000*24))*(3600000*24));break;
			case "hour": url=baseUrl+"/"+(Math.floor(Date.now()/(3600000))*(3600000));break;
			case "all":
			default:
				url=baseUrl;
		}
		download(url);
	}
	return chart;
}

function wdSizedComponant(pClass, pW, pH) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdBaseComponant(),
		width	= (typeof pW!="undefined"&&pW!=null)?pW:0, 
		height	= (typeof pH!="undefined"&&pH!=null)?pH:0;
	chart.dispatch.register("heightUpdate","widthUpdate");
	chart.width	= function(_) { 
		if (!arguments.length) return width; width = _;
		chart.dispatch.call("widthUpdate");
		return chart;
	}
	chart.height	= function(_) { 
		if (!arguments.length) return height; height = _;
		chart.dispatch.call("heightUpdate");
		return chart;
	}
	return chart;
}

function wdMinSizedComponant(pClass, pW, pH) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdSizedComponant(null, pW, pH),
		minWidth	= (typeof pW!="undefined"&&pW!=null)?pW:0, 
		minHeight	= (typeof pH!="undefined"&&pH!=null)?pH:0,
		pWidth		= chart.width,
		pHeight		= chart.height;

	chart.updateSizeFromMin	=function () {pWidth(minWidth);pHeight(minHeight)}
	chart.width	= function(_) { 
		if (!arguments.length) return pWidth(); 
		if (_>minWidth) return pWidth(_);
		return chart;
	}
	chart.height	= function(_) { 
		if (!arguments.length) return pHeight(); 
		if (_>minHeight) return pHeight(_);
		return chart;
	}
	return chart;
}

function wdAxedComponant(pClass, pW, pH) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdSizedComponant(null, pW, pH);
	chart.xAxis		= d3.scaleTime().range([0, chart.width()]);
	chart.yAxis		= d3.scaleLinear().range([chart.height(), 0]);
	chart.dispatch.on("widthUpdate.wdAxedComponant", function() { 
		chart.xAxis.range([0, chart.width()]);
	});
	chart.dispatch.on("heightUpdate.wdAxedComponant", function() { 
		chart.yAxis.range([chart.height(), 0]);
	});
	chart.dispatch.on("dataUpdate.wdAxedComponant", function() { 
		chart.xAxis.domain(d3.extent(chart.data(), function(d) { return d.timestamp; }));
		chart.yAxis.domain([0, d3.max(chart.data(), function(d) {
			var keys;
			if (typeof chart.filter !="undefined")
				keys = Object.keys(d).filter(chart.filter());
			else
				keys = Object.keys(d);
			var vals = keys.map(function (i) {return d[i]});
			return d3.max(vals);
		})]);
	});
	return chart;
}

function wdAxesComponant(pClass, pW, pH) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdAxedComponant(null, pW, pH);
	chart.xAxisLine		= function(g) {
		g.call(d3.axisBottom(chart.xAxis).tickFormat(wdDateFormat));
		g.select(".domain").remove();
		g.selectAll(".tick line").attr("stroke", "lightgrey").style("stroke-width", "1.5px");
	}
	chart.yAxisLine		= function(g) {
		g.call(d3.axisRight(chart.yAxis).tickSize(chart.width()));
		g.select(".domain").remove();
		g.selectAll(".tick line").attr("stroke", "lightgrey").style("stroke-width", "1px");
		g.selectAll(".tick:not(:first-of-type) line").attr("stroke-dasharray", "5,5");
		g.selectAll(".tick text").attr("x", -20);
	};
	chart.dispatch.on("init.wdAxesComponant", function() { 
		chart.root().append("g").attr("class", "x axis").attr("transform", "translate(0," + chart.height() + ")").call(chart.xAxisLine);
		chart.root().append("g").attr("class", "y axis").call(chart.yAxisLine);
	});
	chart.dispatch.on("heightUpdate.wdAxesComponant", function() { 
		if (chart.inited())
			root.select(".x.axis").attr("transform", "translate(0," + chart.height() + ")");
	});
	chart.dispatch.on("renderUpdate.wdAxesComponant", function() { 
		var	update	= chart.root().transition();
		update.select(".x.axis").duration(150).call(chart.xAxisLine);
		update.select(".y.axis").duration(150).call(chart.yAxisLine);
	});
	return chart;
}

function wdHLegendComponant(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdSizedComponant( wdColoredComponant( wdFilteredComponant()), 500,30),
		labels	= [];

	chart.dispatch.register("itemMouseOver","itemMouseOut");
	chart.dispatch.on("init.wdHLegendComponant", function() { 
		if (typeof chart.callbacks.itemMouseOver !== 'undefined')
			chart.dispatch.on("itemMouseOver.legend", chart.callbacks.itemMouseOver);
		if (typeof chart.callbacks.itemMouseOut !== 'undefined')
			chart.dispatch.on("itemMouseOut.legend",  chart.callbacks.itemMouseOut);
	});
	chart.dispatch.on("dataUpdate.wdHLegendComponant", function() { 
		labels = chart.keys().map(function(i) {
			return {
				id: i,
				val: chart.data()[0][i]
			};
		});
		chart.color().domain(chart.keys());
	});
	chart.dispatch.on("dataUpdate.wdHLegendComponant", function() { 
		labels = chart.keys().map(function(i) {
			return {
				id: i,
				val: chart.data()[0][i]
			};
		});
		chart.color().domain(chart.keys());
	});
	chart.dispatch.on("renderUpdate.wdHLegendComponant", function() { 
		chart.root().selectAll('g.legend').remove();
		var	update	= chart.root().selectAll('g.legend').data(labels),
			gEnter	= update.enter().append('g').attr('class', 'legend').attr('id', function (d,i) { return d.id });
		gEnter.append('circle')
			.style('fill', function(d, i){ return chart.color()(d.id) })
			.style('stroke', function(d, i){ return chart.color()(d.id) })
			.attr('r', 5);
		gEnter.append('text')
			.text(function(d) { return d.id+": "+d.val })
			.attr('text-anchor', 'start')
			.attr('dy', '.32em')
			.attr('dx', '8');
		var	x=0, newx=0,y=0;
		gEnter.attr('transform', function(d, i) {
			var length = (Math.floor(d3.select(this).select('text').node().getComputedTextLength()/200)+1)*200;
			x 	 = newx;
			newx	+=length;
			if (newx>chart.width()) {
				x=0;
				newx = length;
				y+=15;
			}
			return 'translate(' + x + ','+y+')'
		})	.on("mouseover", function(d, i){chart.dispatch.call("itemMouseOver", null, d, i);})
			.on("mouseout", function(d, i) {chart.dispatch.call("itemMouseOut",  null, d, i);})
	});
	chart.callbacks.updateValues	= function(v) {
		labels.forEach(function (l) {
			l.val = v[l.id]
			chart.root().select("#"+l.id).select("text").text( l.id+": "+l.val)
		})
	}
	return chart;
}
