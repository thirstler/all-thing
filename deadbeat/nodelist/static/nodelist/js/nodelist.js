var noderows = {};
var node_update_interval = null;
var poll_ts = null;

function render_list(result) {

    var tblroot = document.createElement("table");
    tblroot.setAttribute("class", "table table-striped table-bordered");

    var head = document.createElement("thead");
    tblroot.appendChild(head);
    var row = document.createElement("tr");
    head.appendChild(row);

    var col1 = document.createElement("td");
    col1.innerHTML = "Node Name";
    row.appendChild(col1);

    var col2 = document.createElement("td");
    col2.innerHTML = "CPUs";
    row.appendChild(col2);

    var col3 = document.createElement("td");
    col3.innerHTML = "Sysload (1/5/15 min)";
    row.appendChild(col3);

    var col8 = document.createElement("td");
    col8.innerHTML = "CPU Util.";
    row.appendChild(col8);

    var col4 = document.createElement("td");
    col4.innerHTML = "Mem Inf (<span style='font-weight:700'><span style='color: #002235'>used</span>, <span style='color: #795600'>buffer</span>, <span style='color: #486b00'>cache</span>, <span style='color: gray'>avail</span></span>)";
    row.appendChild(col4);

    var col5 = document.createElement("td");
    col5.innerHTML = "Swap";
    row.appendChild(col5);

    var col6 = document.createElement("td");
    col6.innerHTML = "Net";
    row.appendChild(col6);

    var col7 = document.createElement("td");
    col7.innerHTML = "BlkIO";
    row.appendChild(col7);

    var col9 = document.createElement("td");
    col9.innerHTML = "Chk-in";
    row.appendChild(col9);

    var body = document.createElement("tbody");
    tblroot.appendChild(body);

    $("#page_outter").append(tblroot);

    for(id in result.result) {

        noderows[id] = document.createElement("tr");
        body.appendChild(noderows[id]);

        me = result.result[id];
        me.uuid = id;
        noderows[id] = mk_noderow(noderows[id], me);
        //noderows[id].id = "nl_"+id;

    }

    // Initialize everything
    $(function () {
        $('[data-toggle="popover"]').popover()
    });

    $(function () {
        $('[data-toggle="tooltip"]').tooltip()
    });
    // Keep it going
    node_update_interval = setInterval(node_data_refresh, 5000);
}


function start_nodelist(result)
{
    var atq = new ATQuery();
    atq.add_get("cpus");
    atq.add_get("sysload");
    atq.add_get("rates.cpu_ttls");
    atq.add_get("ts");
    atq.add_get("memory");
    atq.add_get("misc");
    for (n in result) {
        atq.add_uuid(result[n]["uuid"]);
    }
    atq.get(render_list);

}

function nodelist_init() {
    $("#page_outter").empty();
    $.ajax({
        url: "/at_client/at_list",
        success: start_nodelist
    });
}

function node_data_refresh()
{
    refresh = function(result) {
        poll_ts = result["ts"];
        for(uuid in result.result) {
            result.result[uuid].uuid = uuid;
            update_noderow(noderows[uuid], result.result[uuid]);
        }

    };
    var atq = new ATQuery();
    atq.add_get("cpus");
    atq.add_get("sysload");
    atq.add_get("rates.cpu_ttls");
    atq.add_get("ts");
    atq.add_get("memory");
    atq.add_get("misc");
    for (n in noderows) {
        atq.add_uuid(n);
    }
    atq.get(refresh);
}

function mk_noderow(elm, data) {
    // Set name brick
    elm.name = document.createElement("td");
    elm.name.className = "node_brick";
    if (typeof data["misc"] !== 'undefined') {
        var link = document.createElement("a");
        link.appendChild(document.createTextNode(data["misc"]["hostname"]));
        elm.name.appendChild(link);
    } else {
        elm.name.appendChild(document.createTextNode(data["uuid"]));
    }
    elm.appendChild(elm.name);

    // CPU count
    elm.sysinf = document.createElement("td");
    elm.sysinf.className = "node_brick";
    elm.sysinf.innerHTML = '';
    elm.appendChild(elm.sysinf);

    // Set load brick
    elm.sysload = document.createElement("td");
    elm.sysload.className = "node_brick";
    elm.sysload.innerHTML = '';
    elm.appendChild(elm.sysload);

    // Set CPU brick
    elm.cpu = document.createElement("td");
    elm.cpu.style.width = "100px";
    elm.cpu.className = "node_brick";
    elm.cpu.innerHTML = '';
    elm.appendChild(elm.cpu);

    // Meminf
    elm.meminf = document.createElement("td");
    elm.meminf.className = "node_brick";
    elm.meminf.memgraph = document.createElement("div");
    mkMembar(elm.meminf.memgraph, 200);
    elm.appendChild(elm.meminf);
    elm.meminf.appendChild(elm.meminf.memgraph);
    mem_info_tip(elm.meminf.memgraph, data.uuid);

    // Swap
    elm.swinf = document.createElement("td");
    elm.swinf.className = "node_brick";
    elm.swinf.swgr = document.createElement("div");
    mkSwapbar(elm.swinf.swgr, 100);
    elm.appendChild(elm.swinf);
    elm.swinf.appendChild(elm.swinf.swgr);

    // Net
    elm.netinf = document.createElement("td");
    elm.netinf.className = "node_brick";
    elm.appendChild(elm.netinf);
    elm.netinf.innerHTML = "?";

    // BlkIO
    elm.blkio = document.createElement("td");
    elm.blkio.className = "node_brick";
    elm.appendChild(elm.blkio);
    elm.blkio.innerHTML = "?";

    // Last check-in
    elm.chk = document.createElement("td");
    elm.chk.className = "node_brick";
    elm.chk.innerHTML = '';
    elm.appendChild(elm.chk);

    /* Set initial values */
    update_noderow(elm, data);

    return elm;
}

function pad(n) {return n<10 ? '0'+n : n}
function timechk(ts, elm)
{
    var tdiff = 0;
    if(poll_ts != null) tdiff = poll_ts.tv_sec - ts.tv_sec;
    var pt = new Date(ts.tv_sec*1000);

    var timestr;
    if(tdiff > 86400) {
        timestr = (pt.getYear()+1900)+"/"+(pad(pt.getMonth()+1))+"/"+pad(pt.getDate());
    } if (tdiff > 30) {
        timestr = pad(pt.getHours())+":"+pad(pt.getMinutes())+":"+pad(pt.getSeconds());
    } else {
        timestr = "current";
    }

    bgcolor = "#e4f5bc";
    color = "black";
    if(tdiff > 300) {
        bgcolor = "#a30000";
        color = "white";
    } else  if (tdiff > 30 ) {
        bgcolor = "#fbeabc";
        color = "black";
    }
    elm.style.backgroundColor = bgcolor;
    elm.style.color = color;
    elm.innerHTML = timestr;
}

function update_noderow(elm, data) {
    elm.sysinf.innerHTML = data["cpus"].length;
    elm.sysload.innerHTML = data["sysload"]["1min"].toFixed(2) + " / " + data["sysload"]["5min"].toFixed(2)  + " / " + data["sysload"]["15min"].toFixed(2);
    elm.cpu.innerHTML = ((data["rates.cpu_ttls"]["user"] + data["rates.cpu_ttls"]["system"])/data["cpus"].length).toFixed(2)+"%";
    elm.meminf.memgraph.update(data["memory"]);
    elm.swinf.swgr.update(data["memory"]);
    timechk(data["ts"], elm.chk);
}

function mem_info_tip(elm, id)
{
    memtips[id] = document.createElement("div");
    elm.setAttribute("data-toggle", "tooltip");
    elm.setAttribute("data-placement", "right");
    elm.setAttribute("title", "yuck");
    elm.setAttribute("html", "true");
}

function sys_info_tip(link, id)
{
    //link.setAttribute("class", "btn btn-default");
    link.setAttribute("data-toggle", "popover");
    link.setAttribute("data-html", "true");
    link.setAttribute("title", "Memory Info");
    link.setAttribute("data-close", "true");
    link.setAttribute("data-content", id);
    link.setAttribute("href", "#");
}

