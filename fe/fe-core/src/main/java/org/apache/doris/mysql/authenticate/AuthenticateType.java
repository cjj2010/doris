// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package org.apache.doris.mysql.authenticate;

import org.apache.doris.common.Config;
import org.apache.doris.common.LdapConfig;

public enum AuthenticateType {
    DEFAULT,
    LDAP;

    public static AuthenticateType getAuthTypeConfig() {
        // Compatible with previously enabled ldap configuration
        if (LdapConfig.ldap_authentication_enabled) {
            return LDAP;
        }
        switch (Config.authentication_type.toLowerCase()) {
            case "default":
                return DEFAULT;
            case "ldap":
                return LDAP;
            // add other authentication system here
            // case otherAuthType:
            default:
                return DEFAULT;
        }
    }

    public static String getAuthTypeConfigString() {
        String authType = Config.authentication_type.toLowerCase();

        if (LdapConfig.ldap_authentication_enabled) {
            return LDAP.name();
        }

        switch (authType) {
            case "default":
                return DEFAULT.toString();
            case "ldap":
                return LDAP.toString();
            default:
                return authType;
        }
    }

}
