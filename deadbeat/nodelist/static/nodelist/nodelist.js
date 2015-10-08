var noderows = {};
var node_update_interval = null;

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
    col2.innerHTML = "Size Inf";
    row.appendChild(col2);

    var col3 = document.createElement("td");
    col3.innerHTML = "Mem Inf (ttl, used, cached, buf, avail)";
    row.appendChild(col3);

    var col4 = document.createElement("td");
    col4.innerHTML = "Sysload (1/5/15 min)";
    row.appendChild(col4);

    var col5 = document.createElement("td");
    col5.innerHTML = "CPU";
    row.appendChild(col5);

    var body = document.createElement("tbody");
    tblroot.appendChild(body);

    $("#page_outter").append(tblroot);

    for(id in result.result) {
        noderows[id] = document.createElement("tr");
        body.appendChild(noderows[id]);

        me = result.result[id];
        noderows[id] = mk_noderow(noderows[id], me);
        //noderows[id].id = "nl_"+id;

    }

    // Keep it going
    node_update_interval = setInterval(node_data_refresh, 5000);
}


function start_nodelist(result)
{
    var atq = new ATQuery();
    atq.add_get("cpus");
    atq.add_get("sysload");
    atq.add_get("rates.cpu_ttls");
    atq.add_get("rates.iodev");
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
        for(uuid in result.result) {
            update_noderow(noderows[uuid], result.result[uuid]);
        }
    };
    var atq = new ATQuery();
    atq.add_get("cpus");
    atq.add_get("sysload");
    atq.add_get("rates.cpu_ttls");
    atq.add_get("rates.iodev");
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
    elm.name.appendChild(document.createTextNode(data["misc"]["hostname"]));
    elm.appendChild(elm.name);

    // Sys data
    elm.sysinf = document.createElement("td");
    elm.sysinf.className = "node_brick";
    elm.sysinf.innerHTML = "CPUS: "+ data["cpus"].length + ", Mem: "+humanMemInfo(data["memory"]["MemTotal"]);
    elm.appendChild(elm.sysinf);

    // Meminf
    elm.meminf = document.createElement("td");
    elm.meminf.className = "node_brick";
    elm.meminf.memgraph = document.createElement("div");
    mkMembar(elm.meminf.memgraph, 300);
    elm.appendChild(elm.meminf);
    elm.meminf.appendChild(elm.meminf.memgraph);

    // Set load brick
    elm.sysload = document.createElement("td");
    elm.sysload.className = "node_brick";
    elm.sysload.innerHTML = data["sysload"]["1min"].toFixed(2) + " / " + data["sysload"]["5min"].toFixed(2)  + " / " + data["sysload"]["15min"].toFixed(2);
    elm.appendChild(elm.sysload);

    // Set CPU brick
    elm.cpu = document.createElement("td");
    elm.cpu.className = "node_brick";
    elm.cpu.innerHTML = (data["rates.cpu_ttls"]["user"] + data["rates.cpu_ttls"]["system"]).toFixed(2);
    elm.appendChild(elm.cpu);

    return elm;
}

function update_noderow(elm, data) {
    elm.sysinf.innerHTML = "CPUS: "+ data["cpus"].length + ", Mem: "+humanMemInfo(data["memory"]["MemTotal"]);
    elm.sysload.innerHTML = data["sysload"]["1min"].toFixed(2) + " / " + data["sysload"]["5min"].toFixed(2)  + " / " + data["sysload"]["15min"].toFixed(2);
    elm.cpu.innerHTML = (data["rates.cpu_ttls"]["user"] + data["rates.cpu_ttls"]["system"]).toFixed(2);
    elm.meminf.memgraph.update(data["memory"]);
}
