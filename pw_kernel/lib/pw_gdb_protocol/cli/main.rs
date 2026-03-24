// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

use clap::{Parser, Subcommand};
use pw_gdb_protocol::Client;
use tokio::net::TcpStream;
use tokio_util::compat::TokioAsyncReadCompatExt;

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// GDB server address (default: localhost:1234)
    #[arg(short, long, default_value = "localhost:1234")]
    addr: String,
}

#[derive(Subcommand)]
enum Commands {
    /// Read memory from the target
    ReadMemory {
        /// Start address (hex or decimal)
        #[arg(value_parser = parse_int)]
        address: u64,

        /// Length in bytes
        #[arg(value_parser = parse_int)]
        length: u64,
    },
}

fn parse_int(s: &str) -> Result<u64, String> {
    if let Some(hex_str) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        u64::from_str_radix(hex_str, 16).map_err(|e| e.to_string())
    } else {
        s.parse::<u64>().map_err(|e| e.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_int() {
        assert_eq!(parse_int("10").unwrap(), 10);
        assert_eq!(parse_int("0x10").unwrap(), 16);
        assert_eq!(parse_int("0X10").unwrap(), 16);
        assert!(parse_int("invalid").is_err());
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn core::error::Error>> {
    let cli = Cli::parse();

    let stream = TcpStream::connect(&cli.addr).await?;
    let compat_stream = stream.compat();
    let mut client = Client::new(compat_stream);

    match cli.command {
        Commands::ReadMemory { address, length } => {
            let data = client.read_memory(address, length).await?;
            println!("{}", hex::encode(data));
        }
    }

    Ok(())
}
