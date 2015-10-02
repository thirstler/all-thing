from django.conf.urls import patterns, include, url
from settings import views

urlpatterns = patterns('',
    url(r'^$', views.settings, name='settings'),
)
