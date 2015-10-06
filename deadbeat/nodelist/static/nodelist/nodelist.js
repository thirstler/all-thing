

function start_nodelist(result)
{
    var node_query = {
        get: [],
        uuid: []
    }
    node_query.get.push("_ALL_");
    for (n in result) {
        node_query.uuid.push(result[n]["uuid"]);
    }

    $.ajax({
        url: 
    })

    $("#page_outter").html(JSON.stringify(node_query));
}

function nodelist_init() {
    $.ajax({
        url: "/at_client/at_list",
        success: start_nodelist
    });
}
