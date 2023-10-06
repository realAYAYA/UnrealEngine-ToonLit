from django.shortcuts import render, redirect
from django.http.response import JsonResponse
from django.http import JsonResponse
from django.http import StreamingHttpResponse
from rest_framework.decorators import api_view, parser_classes
from rest_framework.parsers import FileUploadParser
import time
import logging

logger = logging.getLogger('django')

@api_view(['GET', 'POST', 'DELETE', 'PUT'])
def methods(request):
    return JsonResponse({})

@api_view(['GET'])
def get_large_response_without_chunks(request, bytes_number):
    data = "d" * bytes_number
    return JsonResponse({'data' : data})

@api_view(['GET'])
def nonstreaming_receivetimeout(request, wait_time):
    time.sleep(wait_time)
    return JsonResponse({})

@api_view(['GET'])
def streaming_download(request, chunks, chunk_size):
    def data_chunk_generator():
        for x in range(chunks):
            data_chunk = 'd' * chunk_size
            yield data_chunk
    response = StreamingHttpResponse(data_chunk_generator())
    response['Content-Length'] = chunks * chunk_size
    return response

def output_file_info(f):
    if f.multiple_chunks():
        # big file
        total_chunks = 0
        total_bytes = 0
        for chunk in f.chunks():
            total_chunks += 1
            total_bytes += len(chunk)
        logger.info("Received %d chunks: %d bytes in total", total_chunks, total_bytes)
    else:
        # small file
        logger.info("Received the whole file: %d bytes", f.size)

@api_view(['POST'])
def streaming_upload_post(request):
    f = request.FILES['file']
    output_file_info(f)
    return JsonResponse({})

@api_view(['PUT'])
@parser_classes((FileUploadParser,))
def streaming_upload_put(request):
    f = request.data['file']
    output_file_info(f)
    return JsonResponse({})

@api_view(['GET'])
def redirect_from(request):
    return redirect('redirect_to')

@api_view(['GET'])
def redirect_to(request):
    return JsonResponse({})
