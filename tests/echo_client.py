#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Echo Server 测试客户端
用法：
  python echo_client.py <服务器IP> <端口> [消息]
  
示例：
  python echo_client.py 127.0.0.1 8888 "Hello Server"
  python echo_client.py <服务器IP> 8888
"""

import socket
import sys
import time

def test_echo(host, port, message="Hello Echo Server!"):
    """测试 Echo 服务器"""
    try:
        # 创建 socket
        print(f"[客户端] 正在连接到 {host}:{port}...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 设置 5 秒超时
        
        # 连接服务器
        start_time = time.time()
        sock.connect((host, port))
        connect_time = time.time() - start_time
        print(f"[客户端] ✓ 连接成功！ (耗时: {connect_time:.3f}秒)")
        
        # 发送消息
        message_bytes = message.encode('utf-8')
        print(f"[客户端] 发送消息: {message} ({len(message_bytes)} 字节)")
        sock.sendall(message_bytes + b'\n')
        
        # 接收回显
        print(f"[客户端] 等待服务器响应...")
        received = sock.recv(1024)
        
        if received:
            received_str = received.decode('utf-8').strip()
            print(f"[客户端] ✓ 收到回显: {received_str} ({len(received)} 字节)")
            
            # 验证是否正确回显
            if received_str == message:
                print(f"[客户端] ✓✓✓ Echo 测试成功！消息完全匹配！")
                return True
            else:
                print(f"[客户端] ✗ 警告：回显内容与发送内容不完全匹配")
                return False
        else:
            print(f"[客户端] ✗ 服务器未返回任何数据")
            return False
            
    except socket.timeout:
        print(f"[客户端] ✗ 连接超时！请检查：")
        print(f"  1. 服务器是否在运行？")
        print(f"  2. IP 地址和端口是否正确？")
        print(f"  3. 防火墙/安全组是否开放了端口 {port}？")
        return False
        
    except ConnectionRefusedError:
        print(f"[客户端] ✗ 连接被拒绝！")
        print(f"  可能原因：服务器未在 {host}:{port} 上监听")
        return False
        
    except Exception as e:
        print(f"[客户端] ✗ 错误: {type(e).__name__}: {e}")
        return False
        
    finally:
        sock.close()
        print(f"[客户端] 连接已关闭")


def continuous_test(host, port, count=5, interval=1):
    """持续测试"""
    print(f"\n{'='*60}")
    print(f"开始连续测试：{count} 次，间隔 {interval} 秒")
    print(f"{'='*60}\n")
    
    success_count = 0
    for i in range(count):
        print(f"\n--- 第 {i+1}/{count} 次测试 ---")
        message = f"Test message #{i+1} at {time.strftime('%H:%M:%S')}"
        if test_echo(host, port, message):
            success_count += 1
        
        if i < count - 1:  # 最后一次不需要等待
            time.sleep(interval)
    
    print(f"\n{'='*60}")
    print(f"测试完成：{success_count}/{count} 次成功")
    print(f"{'='*60}\n")


def main():
    if len(sys.argv) < 3:
        print("用法: python echo_client.py <服务器IP> <端口> [消息]")
        print("\n示例:")
        print("  python echo_client.py 127.0.0.1 8888")
        print("  python echo_client.py 127.0.0.1 8888 'Hello World'")
        print("  python echo_client.py <服务器IP> 8888")
        print("\n高级选项:")
        print("  python echo_client.py <IP> <端口> --continuous <次数>")
        sys.exit(1)
    
    host = sys.argv[1]
    port = int(sys.argv[2])
    
    # 检查是否是连续测试模式
    if len(sys.argv) > 3 and sys.argv[3] == '--continuous':
        count = int(sys.argv[4]) if len(sys.argv) > 4 else 5
        continuous_test(host, port, count)
    else:
        # 单次测试
        message = sys.argv[3] if len(sys.argv) > 3 else "Hello Echo Server!"
        test_echo(host, port, message)


if __name__ == "__main__":
    main()

