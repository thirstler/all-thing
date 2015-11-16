function dashboard_init() {
    $.ajax({
        url: "/at_client/at_stats",
        success: function(result){
            $("#page_outter").html(result["ru_nvcsw"])
        }
    });
}

