from django.shortcuts import render
from django.http import HttpResponse

# Create your views here.

def index(request):

    return HttpResponse("Sod off")


def auth(request):

    return HttpResponse("Auth off")


def logout(request):

    return HttpResponse("Log off")