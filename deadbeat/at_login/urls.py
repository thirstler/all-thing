from django.conf.urls import patterns, include, url
from at_login import views

urlpatterns = patterns('',
    url(r'^$', views.index, name='login_index'),
    url(r'^auth/', views.auth, name='login_auth'),
    url(r'^logout/', views.at_logout, name='login_logout')
)
