function handleRequest(request, response) {
  response.processAsync();
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);

  response.write(
    '<!DOCTYPE html>' +
    '<div></div>' +
    '<script>' +
    'function doWrite() {' +
    '  document.write("<p></p>");' +
    '  parent.done();' +
    '  document.close();' +
    '}' +
    'setTimeout(doWrite, 1);' +
    '</script>' 
  );

  response.bodyOutputStream.flush();
  // leave the stream open
}

