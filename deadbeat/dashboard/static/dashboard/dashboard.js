window.onload = function() {

    $.ajax({
        url: "/at_client/at_stats",
        success: function(result){
            $("#dashboard_outter").html(result["ru_nvcsw"])
        }
    });
}