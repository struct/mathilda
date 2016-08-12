## Copyright 2015/2016 Yahoo Inc.
## Licensed under the BSD license, see LICENSE file for terms.
## Written by Chris Rohlf
## Rathilda is a Ruby FFI binding for libmathilda
require 'ffi'

MATHILDA_SO_PATH = '../build/libmathilda.so'

class Response < FFI::Struct
	layout :text, :pointer,
	:size, :uint32
end

class Rathilda
	extend FFI::Library
	ffi_lib MATHILDA_SO_PATH

	attach_function :new_mathilda, [], :pointer
	attach_function :delete_mathilda, [ :pointer ], :void
	attach_function :mathilda_add_instruction, [ :pointer, :pointer], :void
	attach_function :mathilda_clear_instructions, [ :pointer ], :void
	attach_function :mathilda_wait_loop, [ :pointer ], :void
	attach_function :mathilda_set_use_shm, [ :pointer, :int32 ], :void
	attach_function :mathilda_set_safe_to_fork, [ :pointer, :int32 ], :void
	attach_function :mathilda_set_cpu, [ :pointer, :int32 ], :void
	attach_function :mathilda_set_slow_parallel, [ :pointer, :int32 ], :void
	attach_function :mathilda_set_process_timeout, [ :pointer, :int32 ], :void
	attach_function :mathilda_set_shm_sz, [ :pointer, :int32 ], :void
	attach_function :mathilda_execute_instructions, [ :pointer ], :int32, :blocking => true
	attach_function :mathilda_get_shm_id, [ :pointer ], :int32
	attach_function :mathilda_get_shm_ptr, [ :pointer ], :pointer
	callback :finish_function, [:pointer ], :void
	attach_function :mathilda_set_finish, [ :pointer, :finish_function ], :void

	attr_accessor :mathilda

	def initialize
		@mathilda = new_mathilda
	end

	def add_instruction(i)
		mathilda_add_instruction(@mathilda, i.instruction)
	end

	def method_missing(m, *args, &block)
		eval "mathilda_#{m}(@mathilda, *args)"
	end
end

class Instruction
	extend FFI::Library
	ffi_lib MATHILDA_SO_PATH

	attach_function :new_instruction, [ :string, :string ], :pointer
	attach_function :delete_instruction, [ :pointer ], :void
	attach_function :instruction_add_http_header, [ :pointer , :string ], :int32
	attach_function :instruction_set_user_agent, [ :pointer, :string ], :int32
	attach_function :instruction_set_host, [ :pointer , :string ], :void
	attach_function :instruction_set_path, [ :pointer, :string ], :void
	attach_function :instruction_set_http_method, [ :pointer, :string ], :void
	attach_function :instruction_set_post_body, [ :pointer, :string ], :void
	attach_function :instruction_set_cookie_file, [ :pointer, :string ], :void
	attach_function :instruction_set_proxy, [ :pointer, :string ], :void
	attach_function :instruction_set_ssl, [ :pointer, :int32 ], :void
	attach_function :instruction_set_include_headers, [ :pointer, :int32 ], :void
	attach_function :instruction_set_follow_redirects, [ :pointer, :int32 ], :void
	attach_function :instruction_set_use_proxy, [ :pointer, :int32 ], :void
	attach_function :instruction_set_verbose, [ :pointer, :int32 ], :void
	attach_function :instruction_set_port, [ :pointer, :int16 ], :void
	attach_function :instruction_set_proxy_port, [ :pointer, :int16 ], :void
	attach_function :instruction_set_response_code, [ :pointer, :uint32 ], :void
	attach_function :instruction_set_connect_timeout, [ :pointer, :uint32 ], :void
	attach_function :instruction_set_http_timeout, [ :pointer, :uint32 ], :void
	callback :before_function, [ :pointer, :pointer ], :void
	attach_function :instruction_set_before, [ :pointer, :before_function ], :void
	callback :after_function, [ :pointer, :pointer, :pointer ], :void
	attach_function :instruction_set_after, [ :pointer, :after_function ], :void
	attach_function :instruction_get_response, [ :pointer ], :pointer
	attach_function :instruction_get_curl_code, [ :pointer ], :int

	attr_accessor :instruction, :host, :path

	def initialize(host, path)
		return nil if !host.kind_of?(String) or !path.kind_of?(String)
		@host = host
		@path = path
		@instruction = new_instruction(host, path)
	end

	def method_missing(m, *args, &block)
		eval "instruction_#{m}(@instruction, *args)"
	end
end

class RathildaUtils
	extend FFI::Library
	ffi_lib MATHILDA_SO_PATH

	## Many of the methods in MathildaUtils are better
	## implemented in Ruby itself instead of the binding
	attach_function :util_link_blacklist, [ :string ], :int
	attach_function :util_page_blacklist, [ :string ], :int
	attach_function :util_is_http_uri, [ :string ], :int
	attach_function :util_is_https_uri, [ :string ], :int
	attach_function :util_is_subdomain, [ :string ], :int
	attach_function :util_is_domain_host, [ :string, :string ], :int
	attach_function :util_extract_host_from_uri, [ :string ], :pointer
	attach_function :util_extract_path_from_uri, [ :string ], :pointer
	attach_function :util_normalize_uri, [ :string ], :pointer
end

class RathildaDNS
	extend FFI::Library
	ffi_lib MATHILDA_SO_PATH

	attach_function :new_mathildadns, [ ], :pointer
	attach_function :delete_mathildadns, [ :pointer ], :void
	attach_function :mathildadns_flush_cache, [ :pointer ], :void
	attach_function :mathildadns_disable_cache, [ :pointer ], :void
	attach_function :mathildadns_enable_cache, [ :pointer ], :void
	attach_function :mathildadns_name_to_addr, [ :pointer, :string, :int ], :pointer
	attach_function :mathildadns_addr_to_name, [ :pointer, :string ], :pointer
	attach_function :mathildadns_name_to_addr_a, [ :pointer, :string ], :pointer, :blocking => true
	attach_function :mathildadns_addr_to_name_a, [ :pointer, :string ], :pointer, :blocking => true

	attr_accessor :mathildadns

	def initialize
		@mathildadns = new_mathildadns
	end

	def method_missing(m, *args, &block)
		ret = eval "mathildadns_#{m}(@mathildadns, *args)"

		if m =~ /(name_to_addr|addr_to_name)/
			return ret.read_string
		else
			return ret
		end
	end
end

## Example testing code
if __FILE__ == $0
	mdns = RathildaDNS.new
	puts mdns.name_to_addr("yahoo.com", 0)
	puts mdns.addr_to_name("4.4.4.4")
	puts mdns.name_to_addr_a("yahoo.com,cbs.com,golf.com")
	puts mdns.addr_to_name_a("4.4.4.4,8.8.8.8,1.2.3.4")

	r = Rathilda.new

	after_callback = Proc.new do |a,b,c|
		r = Response.new(c)
		puts r[:text].read_string
	end

	i = Instruction.new("yahoo.com", "/")
	i.set_after(after_callback)
	r.add_instruction(i)
	r.execute_instructions
end