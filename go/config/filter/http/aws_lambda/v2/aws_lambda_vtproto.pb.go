//go:build vtprotobuf
// +build vtprotobuf

// Code generated by protoc-gen-go-vtproto. DO NOT EDIT.
// source: api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.proto

package v2

import (
	protohelpers "github.com/planetscale/vtprotobuf/protohelpers"
	durationpb "github.com/planetscale/vtprotobuf/types/known/durationpb"
	wrapperspb "github.com/planetscale/vtprotobuf/types/known/wrapperspb"
	proto "google.golang.org/protobuf/proto"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

func (m *AWSLambdaPerRoute) MarshalVTStrict() (dAtA []byte, err error) {
	if m == nil {
		return nil, nil
	}
	size := m.SizeVT()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBufferVTStrict(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *AWSLambdaPerRoute) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaPerRoute) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	if m == nil {
		return 0, nil
	}
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.unknownFields != nil {
		i -= len(m.unknownFields)
		copy(dAtA[i:], m.unknownFields)
	}
	if m.RequestTransformerConfig != nil {
		if vtmsg, ok := interface{}(m.RequestTransformerConfig).(interface {
			MarshalToSizedBufferVTStrict([]byte) (int, error)
		}); ok {
			size, err := vtmsg.MarshalToSizedBufferVTStrict(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		} else {
			encoded, err := proto.Marshal(m.RequestTransformerConfig)
			if err != nil {
				return 0, err
			}
			i -= len(encoded)
			copy(dAtA[i:], encoded)
			i = protohelpers.EncodeVarint(dAtA, i, uint64(len(encoded)))
		}
		i--
		dAtA[i] = 0x3a
	}
	if m.TransformerConfig != nil {
		if vtmsg, ok := interface{}(m.TransformerConfig).(interface {
			MarshalToSizedBufferVTStrict([]byte) (int, error)
		}); ok {
			size, err := vtmsg.MarshalToSizedBufferVTStrict(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		} else {
			encoded, err := proto.Marshal(m.TransformerConfig)
			if err != nil {
				return 0, err
			}
			i -= len(encoded)
			copy(dAtA[i:], encoded)
			i = protohelpers.EncodeVarint(dAtA, i, uint64(len(encoded)))
		}
		i--
		dAtA[i] = 0x32
	}
	if m.UnwrapAsAlb {
		i--
		if m.UnwrapAsAlb {
			dAtA[i] = 1
		} else {
			dAtA[i] = 0
		}
		i--
		dAtA[i] = 0x28
	}
	if m.EmptyBodyOverride != nil {
		size, err := (*wrapperspb.StringValue)(m.EmptyBodyOverride).MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
		i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		i--
		dAtA[i] = 0x22
	}
	if m.Async {
		i--
		if m.Async {
			dAtA[i] = 1
		} else {
			dAtA[i] = 0
		}
		i--
		dAtA[i] = 0x18
	}
	if len(m.Qualifier) > 0 {
		i -= len(m.Qualifier)
		copy(dAtA[i:], m.Qualifier)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Qualifier)))
		i--
		dAtA[i] = 0x12
	}
	if len(m.Name) > 0 {
		i -= len(m.Name)
		copy(dAtA[i:], m.Name)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Name)))
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}

func (m *AWSLambdaProtocolExtension) MarshalVTStrict() (dAtA []byte, err error) {
	if m == nil {
		return nil, nil
	}
	size := m.SizeVT()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBufferVTStrict(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *AWSLambdaProtocolExtension) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaProtocolExtension) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	if m == nil {
		return 0, nil
	}
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.unknownFields != nil {
		i -= len(m.unknownFields)
		copy(dAtA[i:], m.unknownFields)
	}
	if m.DisableRoleChaining {
		i--
		if m.DisableRoleChaining {
			dAtA[i] = 1
		} else {
			dAtA[i] = 0
		}
		i--
		dAtA[i] = 0x38
	}
	if len(m.RoleArn) > 0 {
		i -= len(m.RoleArn)
		copy(dAtA[i:], m.RoleArn)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.RoleArn)))
		i--
		dAtA[i] = 0x32
	}
	if len(m.SessionToken) > 0 {
		i -= len(m.SessionToken)
		copy(dAtA[i:], m.SessionToken)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.SessionToken)))
		i--
		dAtA[i] = 0x2a
	}
	if len(m.SecretKey) > 0 {
		i -= len(m.SecretKey)
		copy(dAtA[i:], m.SecretKey)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.SecretKey)))
		i--
		dAtA[i] = 0x22
	}
	if len(m.AccessKey) > 0 {
		i -= len(m.AccessKey)
		copy(dAtA[i:], m.AccessKey)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.AccessKey)))
		i--
		dAtA[i] = 0x1a
	}
	if len(m.Region) > 0 {
		i -= len(m.Region)
		copy(dAtA[i:], m.Region)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Region)))
		i--
		dAtA[i] = 0x12
	}
	if len(m.Host) > 0 {
		i -= len(m.Host)
		copy(dAtA[i:], m.Host)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Host)))
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}

func (m *AWSLambdaConfig_ServiceAccountCredentials) MarshalVTStrict() (dAtA []byte, err error) {
	if m == nil {
		return nil, nil
	}
	size := m.SizeVT()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBufferVTStrict(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *AWSLambdaConfig_ServiceAccountCredentials) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaConfig_ServiceAccountCredentials) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	if m == nil {
		return 0, nil
	}
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.unknownFields != nil {
		i -= len(m.unknownFields)
		copy(dAtA[i:], m.unknownFields)
	}
	if len(m.Region) > 0 {
		i -= len(m.Region)
		copy(dAtA[i:], m.Region)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Region)))
		i--
		dAtA[i] = 0x22
	}
	if m.Timeout != nil {
		size, err := (*durationpb.Duration)(m.Timeout).MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
		i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		i--
		dAtA[i] = 0x1a
	}
	if len(m.Uri) > 0 {
		i -= len(m.Uri)
		copy(dAtA[i:], m.Uri)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Uri)))
		i--
		dAtA[i] = 0x12
	}
	if len(m.Cluster) > 0 {
		i -= len(m.Cluster)
		copy(dAtA[i:], m.Cluster)
		i = protohelpers.EncodeVarint(dAtA, i, uint64(len(m.Cluster)))
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}

func (m *AWSLambdaConfig) MarshalVTStrict() (dAtA []byte, err error) {
	if m == nil {
		return nil, nil
	}
	size := m.SizeVT()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBufferVTStrict(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *AWSLambdaConfig) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaConfig) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	if m == nil {
		return 0, nil
	}
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.unknownFields != nil {
		i -= len(m.unknownFields)
		copy(dAtA[i:], m.unknownFields)
	}
	if m.CredentialRefreshDelay != nil {
		size, err := (*durationpb.Duration)(m.CredentialRefreshDelay).MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
		i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		i--
		dAtA[i] = 0x22
	}
	if m.PropagateOriginalRouting {
		i--
		if m.PropagateOriginalRouting {
			dAtA[i] = 1
		} else {
			dAtA[i] = 0
		}
		i--
		dAtA[i] = 0x18
	}
	if msg, ok := m.CredentialsFetcher.(*AWSLambdaConfig_ServiceAccountCredentials_); ok {
		size, err := msg.MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
	}
	if msg, ok := m.CredentialsFetcher.(*AWSLambdaConfig_UseDefaultCredentials); ok {
		size, err := msg.MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
	}
	return len(dAtA) - i, nil
}

func (m *AWSLambdaConfig_UseDefaultCredentials) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaConfig_UseDefaultCredentials) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	i := len(dAtA)
	if m.UseDefaultCredentials != nil {
		size, err := (*wrapperspb.BoolValue)(m.UseDefaultCredentials).MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
		i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		i--
		dAtA[i] = 0xa
	} else {
		i = protohelpers.EncodeVarint(dAtA, i, 0)
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}
func (m *AWSLambdaConfig_ServiceAccountCredentials_) MarshalToVTStrict(dAtA []byte) (int, error) {
	size := m.SizeVT()
	return m.MarshalToSizedBufferVTStrict(dAtA[:size])
}

func (m *AWSLambdaConfig_ServiceAccountCredentials_) MarshalToSizedBufferVTStrict(dAtA []byte) (int, error) {
	i := len(dAtA)
	if m.ServiceAccountCredentials != nil {
		size, err := m.ServiceAccountCredentials.MarshalToSizedBufferVTStrict(dAtA[:i])
		if err != nil {
			return 0, err
		}
		i -= size
		i = protohelpers.EncodeVarint(dAtA, i, uint64(size))
		i--
		dAtA[i] = 0x12
	} else {
		i = protohelpers.EncodeVarint(dAtA, i, 0)
		i--
		dAtA[i] = 0x12
	}
	return len(dAtA) - i, nil
}
func (m *AWSLambdaPerRoute) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	l = len(m.Name)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.Qualifier)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	if m.Async {
		n += 2
	}
	if m.EmptyBodyOverride != nil {
		l = (*wrapperspb.StringValue)(m.EmptyBodyOverride).SizeVT()
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	if m.UnwrapAsAlb {
		n += 2
	}
	if m.TransformerConfig != nil {
		if size, ok := interface{}(m.TransformerConfig).(interface {
			SizeVT() int
		}); ok {
			l = size.SizeVT()
		} else {
			l = proto.Size(m.TransformerConfig)
		}
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	if m.RequestTransformerConfig != nil {
		if size, ok := interface{}(m.RequestTransformerConfig).(interface {
			SizeVT() int
		}); ok {
			l = size.SizeVT()
		} else {
			l = proto.Size(m.RequestTransformerConfig)
		}
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	n += len(m.unknownFields)
	return n
}

func (m *AWSLambdaProtocolExtension) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	l = len(m.Host)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.Region)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.AccessKey)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.SecretKey)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.SessionToken)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.RoleArn)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	if m.DisableRoleChaining {
		n += 2
	}
	n += len(m.unknownFields)
	return n
}

func (m *AWSLambdaConfig_ServiceAccountCredentials) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	l = len(m.Cluster)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.Uri)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	if m.Timeout != nil {
		l = (*durationpb.Duration)(m.Timeout).SizeVT()
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	l = len(m.Region)
	if l > 0 {
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	n += len(m.unknownFields)
	return n
}

func (m *AWSLambdaConfig) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if vtmsg, ok := m.CredentialsFetcher.(interface{ SizeVT() int }); ok {
		n += vtmsg.SizeVT()
	}
	if m.PropagateOriginalRouting {
		n += 2
	}
	if m.CredentialRefreshDelay != nil {
		l = (*durationpb.Duration)(m.CredentialRefreshDelay).SizeVT()
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	}
	n += len(m.unknownFields)
	return n
}

func (m *AWSLambdaConfig_UseDefaultCredentials) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.UseDefaultCredentials != nil {
		l = (*wrapperspb.BoolValue)(m.UseDefaultCredentials).SizeVT()
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	} else {
		n += 2
	}
	return n
}
func (m *AWSLambdaConfig_ServiceAccountCredentials_) SizeVT() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.ServiceAccountCredentials != nil {
		l = m.ServiceAccountCredentials.SizeVT()
		n += 1 + l + protohelpers.SizeOfVarint(uint64(l))
	} else {
		n += 2
	}
	return n
}