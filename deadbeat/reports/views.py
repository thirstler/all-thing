from django.http import HttpResponse
from django.contrib.auth.decorators import login_required
from django.template import RequestContext, loader

@login_required
def reports(request, message=None):
    template = loader.get_template('allthing/index.html')
    context = RequestContext(request, {
        'reports': True,
        'full_name': "{0}, {1}".format(request.user.last_name, request.user.first_name),
        'message': message,
        'html_title': "AllThing Dashboard",
        'page_heading': "Reports",
        'html_body': 'body test'
    })
    return HttpResponse(template.render(context))