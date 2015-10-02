from django.conf.urls import patterns, include, url
from nodelist import views

urlpatterns = patterns('',
    url(r'^$', views.nodelist, name='nodelist'),
)
