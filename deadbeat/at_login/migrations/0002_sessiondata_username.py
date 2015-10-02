# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import models, migrations


class Migration(migrations.Migration):

    dependencies = [
        ('at_login', '0001_initial'),
    ]

    operations = [
        migrations.AddField(
            model_name='sessiondata',
            name='username',
            field=models.CharField(default=None, max_length=255),
        ),
    ]
