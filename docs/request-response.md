# Request and Response

The request API provides route parameters, query input, URL-encoded forms, JSON bodies, cookies, normalized headers, content negotiation and common HTTP metadata.

Convenience accessors cover content type, host, user agent and bearer authorization. Authentication policy remains outside Request; bearer helpers only parse transport metadata.

Response supports text, JSON, rendered views, HTML, redirects, no-content responses, downloads and cookies.

Cookies are first-class response values with Path, Domain, Max-Age, Secure, HttpOnly and SameSite attributes. Expiring a cookie uses the same cookie abstraction with Max-Age zero.

Downloads set Content-Disposition and an explicit content type. Large-file zero-copy/sendfile support belongs to the storage/production transport integration rather than buffering policy in the high-level Response API.

Uploaded-file multipart parsing remains a transport/parser concern and should integrate with the existing UploadedFile abstraction without making Request responsible for filesystem persistence.
