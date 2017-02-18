/////////////////////////////////////////////////////////////////////////////////////////////
// watchedEvent

function wdLineLines(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdColoredComponant( wdFilteredComponant( wdAxedComponant(null, 500,200))),
		line		= d3.line().curve(d3.curveBasis)
					.x(function(d) { return chart.xAxis(d.timestamp); })
					.y(function(d) { return chart.yAxis(d.value); }),
		lines		= [];

	chart.dispatch.on("renderUpdate.wdLineLines", function() {
		chart.root().selectAll(".lines").remove();
		var	update	= chart.root().selectAll(".lines").data(lines, function(d) { return d.timestamp }),
			eLines	= update.enter().append("g").attr("class", "lines");
		update.exit().remove();
		eLines.append("path").attr("class", "line")
			.attr("d", function(d) { return line(d.values); })
			.style("stroke", function(d) { return chart.color()(d.id); });
	});
	chart.dispatch.on("dataUpdate.wdLineLines", function() { 
		lines = chart.keys().map(function(i) {
			return {
				id: i,
				values: chart.data().map(function(d) {
					return {timestamp: d.timestamp, value:+d[i]};
				})
			};
		});
		chart.color().domain(chart.keys());
	});
	return chart;
}

function wdLineAxes(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdFilteredComponant( wdAxesComponant( null,500,200)),
		marq;

	chart.callbacks.mouseMove	= function(x,y) {
		if (typeof marq !== 'undefined') marq.attr("x1", x).attr("x2", x);
	};
	chart.dispatch.on("init.wdEventAxes", function() { 
		marq	= chart.root().append("line").attr("x1", 0).attr("y1", 0)
				.attr("x2", 0).attr("y2", chart.height())
				.attr("shape-rendering", "crispEdges")
				.style("stroke-width", 1).style("stroke", "black")
				.style("fill", "none");
	});
	return chart;
}

function wdLineLegend(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass:wdHLegendComponant();
	return chart;
}

function wdLineChart(pClass) {
	var	chart	= (typeof pClass!="undefined"&&pClass!=null)?pClass: wdPeriodComponant( wdColoredComponant( wdFilteredComponant(wdMinSizedComponant(null, 500,380)))),
		margin		= {top: 30, right: 10, bottom: 20, left: 30},
		axes		= wdLineAxes(),
		legend 		= wdLineLegend().color(chart.color()),
		lines		= wdLineLines().color(chart.color()),
		xRev		= d3.scaleTime().domain([0, chart.width()-margin.left-margin.right]),
		baseUrl		= "",
		prop		= "",
		oldX		= 0;

	chart.dispatch.register("updateValues","mouseMove");
	chart.callbacks.mouseMove	= function(x,y) {
		if (Math.abs(x-oldX)<1) return;
		oldX=x;
		var v = chart.data().find(function (d) { return d.timestamp>=xRev(x) });
		chart.dispatch.call("updateValues", null, v);
	};
	chart.dispatch.on("init.wdLineChart", function() { 
		var bound	= chart.root().node().getBoundingClientRect();
		chart.width(bound.width);
		chart.height(bound.height);
		legend.height(margin.top - 2*margin.right)
		var svg		= chart.root().append("svg").attr("width", chart.width()).attr("height", chart.height());
		svg.append("g").attr("transform", "translate(" + margin.left + "," + margin.top + ")").call(lines);
		svg.append("g").attr("transform", "translate(" + margin.left + "," + margin.top + ")").call(axes);
		svg.append("g").attr("transform", "translate(" + margin.right + "," + margin.right + ")").call(legend);
		svg.on("mousemove", function() {
			var 	bBox	= svg.node().getBoundingClientRect(),
				x	= d3.event.pageX-bBox.left-margin.left-window.scrollX,
				y	= d3.event.pageY-bBox.top-margin.top-window.scrollY;
			if (	x>=0 && x<=bBox.right-bBox.left-margin.left-margin.right &&
				y>=0 && y<=bBox.bottom-bBox.top-margin.top-margin.bottom) {
				chart.dispatch.call("mouseMove", null, x,y);
			}
		});
		chart.dispatch.on("mouseMove.axis", axes.callbacks.mouseMove);
		chart.dispatch.on("mouseMove.chart", chart.callbacks.mouseMove);
		chart.dispatch.on("updateValues.legend", legend.callbacks.updateValues);
	});
	chart.dispatch.on("heightUpdate.wdLineChart", function() { 
		axes.height(chart.height() - margin.top - margin.bottom);
		lines.height(chart.height() - margin.top - margin.bottom);
	});
	chart.dispatch.on("widthUpdate.wdLineChart", function() { 
		xRev.domain([0, chart.width()-margin.left-margin.right]);
		axes.width(chart.width() - margin.left - margin.right);
		lines.width(chart.width() - margin.left - margin.right);
		legend.width(chart.width() - 2*margin.right);
	});
	chart.dispatch.on("colorUpdate.wdLineChart", function() { 
		lines.color(chart.color());
		legend.color(chart.color());
	});
	chart.dispatch.on("dataUpdate.wdLineChart", function() { 
		xRev.range([d3.min(chart.data(),function(d){return d.timestamp}),d3.max(chart.data(),function(d){return d.timestamp})]);
		axes.data(chart.data());
		lines.data(chart.data());
		legend.data(chart.data());
		chart.dispatch.call("updateValues", null, chart.data().find(function (d) { return d.timestamp>=xRev(oldX) }));
	});
	chart.prop	= function(_) {
		if (!arguments.length) return prop; prop = _;
		chart.filter(function(e){return e!="timestamp" && (e.match("avg_"+prop) || e==prop );});
		axes.filter(chart.filter());
		lines.filter(chart.filter());
		legend.filter(chart.filter());
		return chart;
	};
	chart.updateSizeFromMin();
	return chart;
}

function watchedEvent(id, baseUrl, prop) {
	var chart = wdLineChart().prop(prop).baseUrl(baseUrl);
	d3.select("#"+id).call(chart);
	return chart;
}
function watchedRessource(id, baseUrl) {
	var chart = wdLineChart().baseUrl(baseUrl);
	d3.select("#"+id).call(chart);
	return chart;
}

function watchedLive(id, baseUrl,freq) {
	var chart = wdLineChart().baseUrl(baseUrl);
	setInterval(function() {
		chart.baseUrl(baseUrl);
	}, 1000*freq)
	d3.select("#"+id).call(chart);
	return chart;
}

