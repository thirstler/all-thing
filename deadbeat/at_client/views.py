from django.shortcuts import render
from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
import socket
import json
import allthing.settings


@login_required


def mk_at_query(hostids, attributes):
    """
    :rtype : basestring
    """
    query_obj = {"uuid": hostids, "get": attributes}

    print(json.dumps(query_obj))

    return "query:"+json.dumps(query_obj)

def get_master_sock():
    """
    :rtype : object
    """
    ats = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ats.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ats.connect((allthing.settings.AT_CLIENT["HOST"], allthing.settings.AT_CLIENT["PORT"]))
    return ats

def get_json_msg(at_sock):
    """
    :param at_sock: open socket object
    :return json text message:
    :rtype: str
    """
    max_chunk_sz = 4096
    msg = b''
    while 1:
        chunk = at_sock.recv(max_chunk_sz)
        msg += chunk
        if chunk[-12:].find(b'<<EOF>>') >= 0: break

    # Chop off the newline and EOF
    return msg[:-9].decode('UTF-8')


def send_query(msg, at_sock):
    """
    :param msg: query text
    :param at_sock: open socket object
    :return: True/(False?)
    :rtype: bool
    """
    msglength = len(msg)+1
    msg += '\n'

    sent = 0
    while sent < msglength:
        sent += at_sock.send(msg.encode('UTF-8')[sent:])

    return True

def at_master_stats(request):
    """
    :param request: django request object
    :return: django HttpResponse object
    """
    ats = get_master_sock()
    send_query("stats", ats)
    json_msg = get_json_msg(ats)
    ats.close()
    return HttpResponse(json_msg, content_type="application/json")


def at_master_list(request):
    """
    :param request: django request object
    :return: django HttpResponse object
    """
    ats = get_master_sock()
    send_query("list", ats)
    json_msg = get_json_msg(ats)
    ats.close()
    return HttpResponse(json_msg, content_type="application/json")


def at_master_dump(request):
    """
    :param request: django request object
    :return: django HttpResponse object
    """
    ats = get_master_sock()
    send_query("list", ats)
    json_msg = get_json_msg(ats)

    syslist = json.loads(json_msg)
    hostar = []
    for s in syslist:
        try: hostar.append(s["id"])
        except: pass

    getme = []
    getme.append("_ALL_")
    query = mk_at_query(hostar, getme)

    send_query(query, ats)
    json_msg = get_json_msg(ats)

    return HttpResponse(json_msg, content_type="application/json")


def raw_query(request):
    try:
        print(str(request))
    except:
        print("Nope")

    ats = get_master_sock()
    send_query("query:{0}".format(request.GET["rawquery"]), ats)
    json_msg = get_json_msg(ats)
    ats.close()
    return HttpResponse(json_msg, content_type="application/json")

def at_master_query(request):

    print(request.GET["hostlist"])
    hostlist = []
    getlist = []
    if request.method == "GET":
        hostlist = request.GET["hostlist"].split(",")
        getlist = request.GET["getlist"].split(",")

    elif request.method == "POST":
        hostlist = request.POST["hostlist"].split(",")
        getlist = request.PoST["getlist"].split(",")

    else:
        return HttpResponse("{}", content_type="application/json")

    query = mk_at_query(hostlist, getlist)
    ats = get_master_sock()
    send_query(query, ats)
    json_msg = get_json_msg(ats)
    ats.close()
    return HttpResponse(json_msg, content_type="application/json")
