# Copyright (C) 2006 Apple Computer, Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = $(WEBCORE_PRIVATE_HEADERS_DIR) $(JAVASCRIPTCORE_PRIVATE_HEADERS_DIR)

PUBLIC_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PUBLIC_HEADERS_FOLDER_PATH)
PRIVATE_HEADERS_DIR = $(TARGET_BUILD_DIR)/$(PRIVATE_HEADERS_FOLDER_PATH)

.PHONY : all
all : \
    $(PRIVATE_HEADERS_DIR)/WebDashboardRegion.h \
    $(PUBLIC_HEADERS_DIR)/WebScriptObject.h \
    $(PUBLIC_HEADERS_DIR)/npapi.h \
    $(PUBLIC_HEADERS_DIR)/npruntime.h \
#

REPLACE_RULES = -e s/WebCore/WebKit/ -e s/JavaScriptCore/WebKit/ -e s/DOMDOMImplementation/DOMImplementation/
HEADER_MIGRATE_CMD = sed $(REPLACE_RULES) $< > $@

$(PUBLIC_HEADERS_DIR)/DOM% : DOMDOM%
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/DOM% : DOMDOM%
	$(HEADER_MIGRATE_CMD)

$(PUBLIC_HEADERS_DIR)/% : %
	$(HEADER_MIGRATE_CMD)

$(PRIVATE_HEADERS_DIR)/% : %
	$(HEADER_MIGRATE_CMD)
