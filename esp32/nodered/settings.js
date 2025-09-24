// nodered/settings.js
const bcrypt = require('bcryptjs');

function envOr(name, def) { return process.env[name] || def; }

module.exports = {
  editorTheme: { projects: { enabled: false } },
  ui: { path: "/ui" },

  // Bảo vệ Editor
  adminAuth: {
    type: "credentials",
    users: [{
      username: envOr("NR_ADMIN_USER","admin"),
      password: envOr("NR_ADMIN_HASH","$2b$08$K9FILL_THIS_WITH_BCRYPT_HASH"),
      permissions: "*"
    }]
  },

  // Bảo vệ HTTP Nodes / Dashboard
  httpNodeAuth: {
    user: envOr("NR_HTTP_USER","user"),
    pass: envOr("NR_HTTP_HASH","$2b$08$K9FILL_THIS_WITH_BCRYPT_HASH")
  },

  // Cho phép Render set PORT động
  uiPort: process.env.PORT || 1880,

  logging: { console: { level: "info" } },

  flowFile: "flows.json",
  functionGlobalContext: {}
};
