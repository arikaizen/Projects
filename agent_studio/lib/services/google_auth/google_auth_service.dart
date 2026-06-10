// Platform-dispatching Google sign-in service.
//   - desktop (dart:io)  → loopback OAuth flow (google_auth_io.dart)
//   - web (dart:html)    → popup OAuth flow    (google_auth_web.dart)
//   - anything else      → unsupported stub    (google_auth_stub.dart)
export 'google_auth_types.dart';

import 'google_auth_stub.dart'
    if (dart.library.io) 'google_auth_io.dart'
    if (dart.library.html) 'google_auth_web.dart';

export 'google_auth_stub.dart'
    if (dart.library.io) 'google_auth_io.dart'
    if (dart.library.html) 'google_auth_web.dart';

final googleAuthService = createGoogleAuthService();
