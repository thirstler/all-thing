from django.shortcuts import render
from django.http import HttpResponse
from django.template import RequestContext, loader
from django.contrib.auth import authenticate, logout, login
from django.shortcuts import redirect
from at_login.models import SessionData
import datetime
import time
# Create your views here.


def index(request, message=None):

    template = loader.get_template('at_login/login.html')

    try:
        redirect = request.GET["next"]
    except:
        redirect = "/"

    msg = msg_status = None
    if message != None:
        try:
            msg_status, msg = message.split(":")
        except:
            msg_status = "info"
            msg = message

    context = RequestContext(request, {
        'message': msg,
        'msg_status': msg_status,
        'html_title': "Log in Required",
        'html_body': 'body test',
        'loginredirect':redirect
    })
    return HttpResponse(template.render(context))


def set_login(username):

    try:
        usess = SessionData.objects.get(username=username)
    except:
        usess = SessionData(username=username,
                            login_time=datetime.datetime.today(),
                            last_activity=datetime.datetime.today())
    return usess


def auth(request):

    user = None
    try:
        user = authenticate(username=request.POST['at_username'], password=request.POST['at_password'])
    except Exception as e:
        index(request, message="login unsuccessful (exception)")


    if user is not None:
        if user.is_active:
            login(request, user)
            set_login(request.POST['at_username'])
            return redirect("/")
        else:
            return index(request, message="login unsuccessful (user not active)")

    else:
        return index(request, message="login unsuccessful (failed username/password)")


def at_logout(request):
    logout(request)
    return index(request, message="success:logout successful")


def main_css(request):
    template = loader.get_template('allthing/main.css')
    context = RequestContext(request, {
        'html_title': "Log in Required",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))
