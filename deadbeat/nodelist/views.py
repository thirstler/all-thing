from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from django.template import RequestContext, loader

@login_required
def nodelist(request, message=None):
    template = loader.get_template('allthing/index.html')
    context = RequestContext(request, {
        'nodelist': True,
        'full_name': "{0}, {1}".format(request.user.last_name, request.user.first_name),
        'message': message,
        'html_title': "AllThing Nodelist",
        'page_heading': "Node List",
        'js_entry': 'nodelist/js/nodelist.js',
        'init_func': 'nodelist_init();'
    })
    return HttpResponse(template.render(context))
