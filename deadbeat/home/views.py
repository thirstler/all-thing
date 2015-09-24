from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from django.template import RequestContext, loader

# Create your views here.
@login_required

def home_landing(request):

    return HttpResponse("you're here")