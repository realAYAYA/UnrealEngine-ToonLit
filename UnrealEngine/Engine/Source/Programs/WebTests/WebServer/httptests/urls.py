from django.urls import path

from . import views

urlpatterns = [
    path("httptests/methods", views.methods, name="methods"),
    path("httptests/get_large_response_without_chunks/<int:bytes_number>/", views.get_large_response_without_chunks, name="get_large_response_without_chunks"),
    path("httptests/nonstreaming_receivetimeout/<int:wait_time>/", views.nonstreaming_receivetimeout, name="nonstreaming_receivetimeout"),
    path("httptests/streaming_download/<int:chunks>/<int:chunk_size>/", views.streaming_download, name="streaming_download"),
    path("httptests/streaming_upload_post", views.streaming_upload_post, name="streaming_upload_post"),
    path("httptests/streaming_upload_put", views.streaming_upload_put, name="streaming_upload_put"),
    path("httptests/redirect_from", views.redirect_from, name="redirect_from"),
    path("httptests/redirect_to", views.redirect_to, name="redirect_to"),
]
