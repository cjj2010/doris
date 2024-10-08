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

package org.apache.doris.nereids.properties;

import org.apache.doris.analysis.SetVar;
import org.apache.doris.analysis.StringLiteral;
import org.apache.doris.nereids.StatementContext;
import org.apache.doris.nereids.exceptions.AnalysisException;
import org.apache.doris.qe.SessionVariable;
import org.apache.doris.qe.VariableMgr;

import java.util.Map;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * select hint.
 * e.g. set_var(query_timeout='1800', exec_mem_limit='2147483648')
 */
public class SelectHintSetVar extends SelectHint {
    // e.g. query_timeout='1800', exec_mem_limit='2147483648'
    private final Map<String, Optional<String>> parameters;

    public SelectHintSetVar(String hintName, Map<String, Optional<String>> parameters) {
        super(hintName);
        this.parameters = parameters;
    }

    public Map<String, Optional<String>> getParameters() {
        return parameters;
    }

    /**
     * set session variable in sql level
     * @param context statement context
     */
    public void setVarOnceInSql(StatementContext context) {
        SessionVariable sessionVariable = context.getConnectContext().getSessionVariable();
        // set temporary session value, and then revert value in the 'finally block' of StmtExecutor#execute
        sessionVariable.setIsSingleSetVar(true);
        for (Map.Entry<String, Optional<String>> kv : getParameters().entrySet()) {
            String key = kv.getKey();
            Optional<String> value = kv.getValue();
            if (value.isPresent()) {
                try {
                    VariableMgr.setVar(sessionVariable, new SetVar(key, new StringLiteral(value.get())));
                    context.invalidCache(key);
                } catch (Throwable t) {
                    throw new AnalysisException("Can not set session variable '"
                        + key + "' = '" + value.get() + "'", t);
                }
            }
        }
    }

    @Override
    public String toString() {
        String kvString = parameters
                .entrySet()
                .stream()
                .map(kv ->
                        kv.getValue().isPresent()
                        ? kv.getKey() + "='" + kv.getValue().get() + "'"
                        : kv.getKey()
                )
                .collect(Collectors.joining(", "));
        return super.getHintName() + "(" + kvString + ")";
    }
}
