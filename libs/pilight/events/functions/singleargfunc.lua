-- Copyright (C) 2018 CurlyMo & Niek

-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

local M = {}

function M.run(a, b)
    if a == nil or b ~= nil then
        error("SINGLEARGFUNC requires one argument")
      end
    return a
end

function M.info()
    return {
        name = "SINGLEARGFUNC",
        version = "1.0",
        reqversion = "7.0",
        reqcommit = "94"
    }
end

return M