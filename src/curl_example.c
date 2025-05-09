#include <stdio.h>
#include <curl/curl.h>

/*

    gcc -o curl_example curl_example.c -lcurl

*/

int main(void) {
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);

    printf("result code: %d\n", res);

    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}