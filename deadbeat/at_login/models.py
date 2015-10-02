from django.db import models


class SessionData(models.Model):
    username = models.CharField(max_length=255, default=None)
    login_time = models.DateTimeField(default=None)
    last_activity = models.DateTimeField(default=None)

