from django.conf.urls import patterns, include, url
from reports import views

urlpatterns = patterns('',
    url(r'^$', views.reports, name='reports'),
)
