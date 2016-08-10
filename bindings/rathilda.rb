## Rathilda is a Ruby FFI binding for libmathilda
require 'ffi'

class Response < FFI::Struct
	layout :text, :pointer,
	:size, :uint32
end

module Mathilda
	extend FFI::Library
	ffi_lib '../build/libmathilda.so'

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
	attach_function :mathilda_execute_instructions, [ :pointer ], :int32
	attach_function :mathilda_get_shm_id, [ :pointer ], :int32
	attach_function :mathilda_get_shm_ptr, [ :pointer ], :pointer

	callback :finish_function, [:pointer ], :void
	attach_function :mathilda_set_finish, [ :pointer, :finish_function ], :void

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
end

## This is a simple example that I intend to build upon
class Rathilda
	attr_accessor :mathilda

	def initialize
		mathilda = Mathilda::new_mathilda
		i = Mathilda::new_instruction("www.yahoo.com", "/")

		p = Proc.new do |a,b,c|
			r = Response.new(c)
			puts r[:text].read_string
		end

		Mathilda::instruction_set_after(i, p)
		Mathilda::mathilda_add_instruction(mathilda, i)
		Mathilda::mathilda_set_safe_to_fork(mathilda, 0)
		Mathilda::mathilda_execute_instructions(mathilda)
	end
end

r = Rathilda.new