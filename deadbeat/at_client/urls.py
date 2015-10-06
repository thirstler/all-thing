from django.conf.urls import patterns, include, url
from at_client import views

urlpatterns = patterns('',
    url(r'^$', views.at_master_stats, name='at_master_stats'),
    url(r'^at_stats$', views.at_master_stats, name='at_master_stats'),
    url(r'^at_list$', views.at_master_list, name='at_master_list'),
    url(r'^at_dump$', views.at_master_dump, name='at_master_dump'),
    url(r'^at_query$', views.at_master_query, name='at_master_query'),
    url(r'^raw_query$', views.raw_query, name='raw_query')
)
