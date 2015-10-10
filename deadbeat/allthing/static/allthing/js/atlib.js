
/* This stuff should be configurable later */


var ATQuery = function() {

    this.query = {
        get: [],
        uuid: []
    };

    this.add_get = function(get) {
        this.query.get.push(get);
    }

    this.add_uuid = function(uuid) {
        this.query.uuid.push(uuid);
    }

    this.query_string = function() {
        return JSON.stringify(this.query);
    }

    this.get = function(callback) {
        $.ajax({
            url: "/at_client/raw_query?rawquery="+this.query_string(),
            success: callback
        });
    }

    this,post = function(callback) {
        $.post("/at_client/raw_query", {rawquery: this.query_string()}, callback );
    }
}

function humanMemInfo(kbytes) {return humanMem(kbytes*1000);}

function humanMem(bytes) {
    if(bytes > Math.pow(2,40)) return ((bytes/Math.pow(10,12)).toFixed(2))+" TB";
    if(bytes > Math.pow(2,30)) return ((bytes/Math.pow(10,9)).toFixed(2))+" GB";
    if(bytes > Math.pow(2,20)) return ((bytes/Math.pow(10,6)).toFixed(2))+" MB";
    if(bytes > Math.pow(2,10)) return ((bytes/Math.pow(10,3)).toFixed(2))+" KB";
    if(bytes > 0) return (bytes)+" B";
    return "Err";
}

var mkSwapbar = function(elm, width)
{
    elm.style.width = width+"px";
    elm.style.backgroundColor = "#ededed";
    elm.meminfo = null;

    elm.swp_used = document.createElement("div");
    elm.appendChild(elm.swp_used);
    elm.swp_used.style.cssFloat = "left";
    elm.swp_used.innerHTML = "<img src='/static/allthing/img/0.gif' />";
    elm.swp_used.style.backgroundColor = "#004368";

    elm.swp_free = document.createElement("div");
    elm.appendChild(elm.swp_free);
    elm.swp_free.style.cssFloat = "left";
    elm.swp_free.innerHTML = "<img src='/static/allthing/img/0.gif' />";
    elm.swp_free.style.backgroundColor = "#ededed";

    elm.update = function(mi) {
        this.meminfo = mi;
        dw = Math.floor(((this.meminfo["SwapTotal"]-this.meminfo["SwapFree"])/this.meminfo["MemTotal"])*width);
        this.swp_used.style.width = dw+"px";
        this.swp_free.style.width = (width-dw)+"px";
    }
}

var mkMembar = function(elm, width)
{
    elm.style.width = width+"px";
    elm.style.backgroundColor = "#ededed";

    elm.meminfo = null;

    elm.mem_used = document.createElement("div");
    elm.appendChild(elm.mem_used);
    elm.mem_used.style.cssFloat = "left";
    elm.mem_used.innerHTML = "<img src='/static/allthing/img/0.gif' />";
    elm.mem_used.style.backgroundColor = "#004368";

    elm.mem_buff = document.createElement("div");
    elm.appendChild(elm.mem_buff);
    elm.mem_buff.style.cssFloat = "left";
    elm.mem_buff.innerHTML =  "<img src='/static/allthing/img/0.gif' />";
    elm.mem_buff.style.backgroundColor = "#f9d67a";

    elm.mem_cache = document.createElement("div");
    elm.appendChild(elm.mem_cache);
    elm.mem_cache.style.cssFloat = "left";
    elm.mem_cache.innerHTML =  "<img src='/static/allthing/img/0.gif' />";
    elm.mem_cache.style.backgroundColor = "#c8eb79";

    elm.mem_free = document.createElement("div");
    elm.appendChild(elm.mem_free);
    elm.mem_free.style.cssFloat = "left";
    elm.mem_free.innerHTML =  "<img src='/static/allthing/img/0.gif' />";
    elm.mem_free.style.backgroundColor = "#ededed";

    elm.update = function(mi) {
        this.meminfo = mi;

        var dw = {
            used: Math.floor(((this.meminfo["MemTotal"]-(this.meminfo["Buffers"] + this.meminfo["Cached"] + this.meminfo["MemFree"]))/this.meminfo["MemTotal"])*width),
            free: Math.floor((this.meminfo["MemFree"]/this.meminfo["MemTotal"])*width),
            buffers: Math.floor((this.meminfo["Buffers"]/this.meminfo["MemTotal"])*width),
            cache: Math.floor((this.meminfo["Cached"]/this.meminfo["MemTotal"])*width)
        };

        var wttl = dw.used+dw.free+dw.buffers+dw.cache;
        var wdiff = width-wttl;

        /* page cache is the best place to shove rounding errors (+/- couple pixels) */
        if ( wdiff > 0) {
            dw.cache += wdiff;
        } else if ( wdiff < 0) {
            dw.cache -= wdiff;
        }

        this.mem_used.style.width = dw.used+"px";
        this.mem_free.style.width = dw.free+"px";
        this.mem_buff.style.width = dw.buffers+"px";
        this.mem_cache.style.width = dw.cache+"px";
    }
}

function mem_tip(meminf)
{
    var outter = document.createElement("div");


}