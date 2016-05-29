--LICENSE			"GPL3"
--DRIVER_AUTHOR		"Roberto de la Cruz, Juan Samper"
--DRIVER_DESC		"Allows comunication between a RBPi and an FPGA SPARTAN 6 using de GPIO RBPi pins"
--					"Receive 16 bits (2 operands, each one of 8 bits), make the substraction and returns the resutl (of 8 bits)"
--
--SUPORTED_DEVICE	"Tested on a FPGA Spartan 3"

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.STD_LOGIC_ARITH.all;

entity communication_protocol is
  port (
    clk_rbpi : in std_logic;
	 direction : in std_logic;
	 dataIn : in std_logic;
	 dataOut : out std_logic);
end communication_protocol;

 
architecture Behavioral of communication_protocol is
	
	signal load : std_logic := '0';
	
	signal buffer_data : std_logic_vector (15 downto 0) := (others => '0');
	signal op1 : std_logic_vector (7 downto 0) := (others => '0');
	signal op2 : std_logic_vector (7 downto 0) := (others => '0');
	signal suma : std_logic_vector (7 downto 0) := (others => '0');
	
	signal resultado : std_logic_vector (7 downto 0) := (others => '0');
	
	signal count : integer := 0;
	
	begin
	
	op1 <= buffer_data(15 downto 8);
	op2 <= buffer_data(7 downto 0);
	
	suma <= op1 - op2;
	
	dataOut <= resultado(7);
	
	MEM_WRITE: process (clk_rbpi) is
	begin
		if rising_edge(clk_rbpi) then
			if count = 16 then
				count <= 0;
			end if;
			if(direction = '1') then
				buffer_data <= dataIn & buffer_data(15 downto 1);
				count <= count + 1;
			end if;
		end if;
	end process MEM_WRITE;
	
	MEM_LOAD: process (clk_rbpi) is
	begin
		if rising_edge(clk_rbpi) then
			if load = '1' then
				load <= '0';
			elsif count = 16 then
				load <= '1';				
			end if;
		end if;
	end process MEM_LOAD;
	
	MEM_READ: process (clk_rbpi) is
	begin
		if rising_edge(clk_rbpi) then
			if(direction = '0') then
				if load = '1' then
					resultado <= suma;
				else
					resultado <= resultado(6 downto 0)&'0';
				end if;
			end if;
		end if;
	end process MEM_READ;
	
end Behavioral;
