"""allthing URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/1.8/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  url(r'^$', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  url(r'^$', Home.as_view(), name='home')
Including another URLconf
    1. Add an import:  from blog import urls as blog_urls
    2. Add a URL to urlpatterns:  url(r'^blog/', include(blog_urls))
"""
from django.conf.urls import include, url
from django.contrib import admin
from allthing import views

urlpatterns = [
    url(r'^admin/', include(admin.site.urls)),
    url(r'^at_client/', include('at_client.urls')),
    url(r'^at_login/', include('at_login.urls')),
    url(r'^dashboard/', include('dashboard.urls')),
    url(r'^nodelist/', include('nodelist.urls')),
    url(r'^reports/', include('reports.urls')),
    url(r'^settings/', include('settings.urls')),
    url(r'^$', include('dashboard.urls'))
]
