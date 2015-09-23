from django.shortcuts import render
from django.http import HttpResponse
from django.template import RequestContext, loader
from django.contrib.auth import authenticate

# Create your views here.

def index(request):
    template = loader.get_template('at_login/index.html')
    context = RequestContext(request, {
        'message': None,
        'html_title': "Log in Required",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))


def auth(request, username, password):
    authed = False
    try:
        authed = authenticate_unpw(username, password)
    except Exception as e:
        raise Http404("Credentials required")

    if authed:
        return HttpResponse("Login successful")
    else:
        return HttpResponse("Login failed")


def logout(request):

    return HttpResponse("Log off")


def authenticate_unpw(username, password):

    user = authenticate(username, password)
    message = None

    if user is not None:
        # the password verified for the user
        if user.is_active:
            return dict(status=True, message="authentication successful")
        else:
            message = "this account has been disabled"

    else:
        # the authentication system was unable to verify the username and password
        message = "the username and/or password were incorrect"

    return dict(status=False, message=message)





def main_css(request):
    template = loader.get_template('allthing/main.css')
    context = RequestContext(request, {
        'html_title': "Log in Required",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))
