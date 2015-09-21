from django.conf.urls import patterns, include, url
from at_login import views

urlpatterns = patterns('',
    url(r'^$', views.login, name='at_login'),
)
