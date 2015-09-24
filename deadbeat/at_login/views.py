from django.shortcuts import render
from django.http import HttpResponse
from django.template import RequestContext, loader
from django.contrib.auth import authenticate, logout

# Create your views here.

def index(request):
    template = loader.get_template('at_login/index.html')
    print("index")
    context = RequestContext(request, {
        'message': None,
        'html_title': "Log in Required",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))


def auth(request):

    user = None
    try:
        user = authenticate(username=request.POST['at_username'], password=request.POST['at_password'])
    except Exception as e:
        print("auth exception: {0}".format(str(e)))

    if user:
        return HttpResponse("Login successful")
    else:
        return HttpResponse("Login failed")


def at_logout(request):
    logout(request)
    return HttpResponse("Log off")


def main_css(request):
    template = loader.get_template('allthing/main.css')
    context = RequestContext(request, {
        'html_title': "Log in Required",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))
