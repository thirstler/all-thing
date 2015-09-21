from django.shortcuts import render
from django.http import HttpResponse

# Create your views here.

def login(request):

    return HttpResponse("Sod off", content_type="text/plain")