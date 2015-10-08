
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

var mkMembar = function(elm, width)
{
    elm.style.width = width+"px";
    elm.style.backgroundColor = "#ededed";

    elm.meminfo = null;

    elm.mem_used = document.createElement("div");
    elm.appendChild(elm.mem_used);
    elm.mem_used.style.cssFloat = "left";
    elm.mem_used.innerHTML = "|";
    elm.mem_used.style.backgroundColor = "#004368";

    elm.mem_buff = document.createElement("div");
    elm.appendChild(elm.mem_buff);
    elm.mem_buff.style.cssFloat = "left";
    elm.mem_buff.innerHTML = "|";
    elm.mem_buff.style.backgroundColor = "#f9d67a";

    elm.mem_cache = document.createElement("div");
    elm.appendChild(elm.mem_cache);
    elm.mem_cache.style.cssFloat = "left";
    elm.mem_cache.innerHTML = "|";
    elm.mem_cache.style.backgroundColor = "#c8eb79";

    elm.mem_free = document.createElement("div");
    elm.appendChild(elm.mem_free);
    elm.mem_free.style.cssFloat = "left";
    elm.mem_free.innerHTML = "|";
    elm.mem_free.style.backgroundColor = "#7dbdc3";

    elm.update = function(mi) {
        this.meminfo = mi;
        this.mem_used.style.width = Math.floor(((this.meminfo["MemTotal"]-(this.meminfo["Buffers"] + this.meminfo["Cached"] + this.meminfo["MemFree"]))/this.meminfo["MemTotal"])*width)+"px";
        this.mem_free.style.width = Math.floor((this.meminfo["MemFree"]/this.meminfo["MemTotal"])*width)+"px";
        this.mem_buff.style.width = Math.floor((this.meminfo["Buffers"]/this.meminfo["MemTotal"])*width)+"px";
        this.mem_cache.style.width = Math.floor((this.meminfo["Cached"]/this.meminfo["MemTotal"])*width)+"px";
    }
}
