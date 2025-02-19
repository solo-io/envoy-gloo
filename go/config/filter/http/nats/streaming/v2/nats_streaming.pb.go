// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.30.0
// 	protoc        v5.29.3
// source: api/envoy/config/filter/http/nats/streaming/v2/nats_streaming.proto

package v2

import (
	_ "github.com/envoyproxy/protoc-gen-validate/validate"
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	durationpb "google.golang.org/protobuf/types/known/durationpb"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

// [#proto-status: experimental]
type NatsStreaming struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Cluster        string               `protobuf:"bytes,1,opt,name=cluster,proto3" json:"cluster,omitempty"`
	MaxConnections uint32               `protobuf:"varint,2,opt,name=max_connections,json=maxConnections,proto3" json:"max_connections,omitempty"`
	OpTimeout      *durationpb.Duration `protobuf:"bytes,3,opt,name=op_timeout,json=opTimeout,proto3" json:"op_timeout,omitempty"`
}

func (x *NatsStreaming) Reset() {
	*x = NatsStreaming{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *NatsStreaming) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*NatsStreaming) ProtoMessage() {}

func (x *NatsStreaming) ProtoReflect() protoreflect.Message {
	mi := &file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use NatsStreaming.ProtoReflect.Descriptor instead.
func (*NatsStreaming) Descriptor() ([]byte, []int) {
	return file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescGZIP(), []int{0}
}

func (x *NatsStreaming) GetCluster() string {
	if x != nil {
		return x.Cluster
	}
	return ""
}

func (x *NatsStreaming) GetMaxConnections() uint32 {
	if x != nil {
		return x.MaxConnections
	}
	return 0
}

func (x *NatsStreaming) GetOpTimeout() *durationpb.Duration {
	if x != nil {
		return x.OpTimeout
	}
	return nil
}

type NatsStreamingPerRoute struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Subject        string `protobuf:"bytes,1,opt,name=subject,proto3" json:"subject,omitempty"`
	ClusterId      string `protobuf:"bytes,2,opt,name=cluster_id,json=clusterId,proto3" json:"cluster_id,omitempty"`
	DiscoverPrefix string `protobuf:"bytes,3,opt,name=discover_prefix,json=discoverPrefix,proto3" json:"discover_prefix,omitempty"`
}

func (x *NatsStreamingPerRoute) Reset() {
	*x = NatsStreamingPerRoute{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *NatsStreamingPerRoute) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*NatsStreamingPerRoute) ProtoMessage() {}

func (x *NatsStreamingPerRoute) ProtoReflect() protoreflect.Message {
	mi := &file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use NatsStreamingPerRoute.ProtoReflect.Descriptor instead.
func (*NatsStreamingPerRoute) Descriptor() ([]byte, []int) {
	return file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescGZIP(), []int{1}
}

func (x *NatsStreamingPerRoute) GetSubject() string {
	if x != nil {
		return x.Subject
	}
	return ""
}

func (x *NatsStreamingPerRoute) GetClusterId() string {
	if x != nil {
		return x.ClusterId
	}
	return ""
}

func (x *NatsStreamingPerRoute) GetDiscoverPrefix() string {
	if x != nil {
		return x.DiscoverPrefix
	}
	return ""
}

var File_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto protoreflect.FileDescriptor

var file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDesc = []byte{
	0x0a, 0x43, 0x61, 0x70, 0x69, 0x2f, 0x65, 0x6e, 0x76, 0x6f, 0x79, 0x2f, 0x63, 0x6f, 0x6e, 0x66,
	0x69, 0x67, 0x2f, 0x66, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x2f, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x6e,
	0x61, 0x74, 0x73, 0x2f, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67, 0x2f, 0x76, 0x32,
	0x2f, 0x6e, 0x61, 0x74, 0x73, 0x5f, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x12, 0x2a, 0x65, 0x6e, 0x76, 0x6f, 0x79, 0x2e, 0x63, 0x6f, 0x6e,
	0x66, 0x69, 0x67, 0x2e, 0x66, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e,
	0x6e, 0x61, 0x74, 0x73, 0x2e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67, 0x2e, 0x76,
	0x32, 0x1a, 0x1e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x62,
	0x75, 0x66, 0x2f, 0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2e, 0x70, 0x72, 0x6f, 0x74,
	0x6f, 0x1a, 0x17, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x61, 0x74, 0x65, 0x2f, 0x76, 0x61, 0x6c, 0x69,
	0x64, 0x61, 0x74, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x95, 0x01, 0x0a, 0x0d, 0x4e,
	0x61, 0x74, 0x73, 0x53, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67, 0x12, 0x21, 0x0a, 0x07,
	0x63, 0x6c, 0x75, 0x73, 0x74, 0x65, 0x72, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x42, 0x07, 0xfa,
	0x42, 0x04, 0x72, 0x02, 0x20, 0x01, 0x52, 0x07, 0x63, 0x6c, 0x75, 0x73, 0x74, 0x65, 0x72, 0x12,
	0x27, 0x0a, 0x0f, 0x6d, 0x61, 0x78, 0x5f, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f,
	0x6e, 0x73, 0x18, 0x02, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x0e, 0x6d, 0x61, 0x78, 0x43, 0x6f, 0x6e,
	0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x12, 0x38, 0x0a, 0x0a, 0x6f, 0x70, 0x5f, 0x74,
	0x69, 0x6d, 0x65, 0x6f, 0x75, 0x74, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x19, 0x2e, 0x67,
	0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x62, 0x75, 0x66, 0x2e, 0x44,
	0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x52, 0x09, 0x6f, 0x70, 0x54, 0x69, 0x6d, 0x65, 0x6f,
	0x75, 0x74, 0x22, 0x94, 0x01, 0x0a, 0x15, 0x4e, 0x61, 0x74, 0x73, 0x53, 0x74, 0x72, 0x65, 0x61,
	0x6d, 0x69, 0x6e, 0x67, 0x50, 0x65, 0x72, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x12, 0x21, 0x0a, 0x07,
	0x73, 0x75, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x42, 0x07, 0xfa,
	0x42, 0x04, 0x72, 0x02, 0x20, 0x01, 0x52, 0x07, 0x73, 0x75, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x12,
	0x26, 0x0a, 0x0a, 0x63, 0x6c, 0x75, 0x73, 0x74, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x02, 0x20,
	0x01, 0x28, 0x09, 0x42, 0x07, 0xfa, 0x42, 0x04, 0x72, 0x02, 0x20, 0x01, 0x52, 0x09, 0x63, 0x6c,
	0x75, 0x73, 0x74, 0x65, 0x72, 0x49, 0x64, 0x12, 0x30, 0x0a, 0x0f, 0x64, 0x69, 0x73, 0x63, 0x6f,
	0x76, 0x65, 0x72, 0x5f, 0x70, 0x72, 0x65, 0x66, 0x69, 0x78, 0x18, 0x03, 0x20, 0x01, 0x28, 0x09,
	0x42, 0x07, 0xfa, 0x42, 0x04, 0x72, 0x02, 0x20, 0x01, 0x52, 0x0e, 0x64, 0x69, 0x73, 0x63, 0x6f,
	0x76, 0x65, 0x72, 0x50, 0x72, 0x65, 0x66, 0x69, 0x78, 0x42, 0x9a, 0x01, 0x0a, 0x38, 0x69, 0x6f,
	0x2e, 0x65, 0x6e, 0x76, 0x6f, 0x79, 0x70, 0x72, 0x6f, 0x78, 0x79, 0x2e, 0x65, 0x6e, 0x76, 0x6f,
	0x79, 0x2e, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x66, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x2e,
	0x68, 0x74, 0x74, 0x70, 0x2e, 0x6e, 0x61, 0x74, 0x73, 0x2e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d,
	0x69, 0x6e, 0x67, 0x2e, 0x76, 0x32, 0x42, 0x12, 0x4e, 0x61, 0x74, 0x73, 0x53, 0x74, 0x72, 0x65,
	0x61, 0x6d, 0x69, 0x6e, 0x67, 0x50, 0x72, 0x6f, 0x74, 0x6f, 0x50, 0x01, 0x5a, 0x48, 0x67, 0x69,
	0x74, 0x68, 0x75, 0x62, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x73, 0x6f, 0x6c, 0x6f, 0x2d, 0x69, 0x6f,
	0x2f, 0x65, 0x6e, 0x76, 0x6f, 0x79, 0x2d, 0x67, 0x6c, 0x6f, 0x6f, 0x2f, 0x67, 0x6f, 0x2f, 0x63,
	0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2f, 0x66, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x2f, 0x68, 0x74, 0x74,
	0x70, 0x2f, 0x6e, 0x61, 0x74, 0x73, 0x2f, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x69, 0x6e, 0x67,
	0x2f, 0x76, 0x32, 0x3b, 0x76, 0x32, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescOnce sync.Once
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescData = file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDesc
)

func file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescGZIP() []byte {
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescOnce.Do(func() {
		file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescData = protoimpl.X.CompressGZIP(file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescData)
	})
	return file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDescData
}

var file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes = make([]protoimpl.MessageInfo, 2)
var file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_goTypes = []interface{}{
	(*NatsStreaming)(nil),         // 0: envoy.config.filter.http.nats.streaming.v2.NatsStreaming
	(*NatsStreamingPerRoute)(nil), // 1: envoy.config.filter.http.nats.streaming.v2.NatsStreamingPerRoute
	(*durationpb.Duration)(nil),   // 2: google.protobuf.Duration
}
var file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_depIdxs = []int32{
	2, // 0: envoy.config.filter.http.nats.streaming.v2.NatsStreaming.op_timeout:type_name -> google.protobuf.Duration
	1, // [1:1] is the sub-list for method output_type
	1, // [1:1] is the sub-list for method input_type
	1, // [1:1] is the sub-list for extension type_name
	1, // [1:1] is the sub-list for extension extendee
	0, // [0:1] is the sub-list for field type_name
}

func init() { file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_init() }
func file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_init() {
	if File_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*NatsStreaming); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes[1].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*NatsStreamingPerRoute); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   2,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_goTypes,
		DependencyIndexes: file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_depIdxs,
		MessageInfos:      file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_msgTypes,
	}.Build()
	File_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto = out.File
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_rawDesc = nil
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_goTypes = nil
	file_api_envoy_config_filter_http_nats_streaming_v2_nats_streaming_proto_depIdxs = nil
}
